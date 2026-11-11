"""Sign-in tests for ha_auth.c against a fresh Home Assistant on BASE:
onboards a user (sam / correcthorse), signs in without and then with a
two-factor code (TOTP set up through the WebSocket API)."""
import asyncio, json, subprocess, sys, time
import pyotp, requests, websockets

BASE, PROG = sys.argv[1], sys.argv[2]
fails = 0

def check(what, got, want):
    global fails
    ok = got == want
    fails += not ok
    print(f"{'ok  ' if ok else 'FAIL'} {what}: {got}" + ("" if ok else f" (want {want})"))

def onboard():
    for _ in range(120):
        try:
            if requests.get(f"{BASE}/api/onboarding", timeout=2).status_code == 200:
                break
        except Exception:
            pass
        time.sleep(2)
    r = requests.post(f"{BASE}/api/onboarding/users", json={"client_id": f"{BASE}/", "name": "Sam",
                      "username": "sam", "password": "correcthorse", "language": "en"})
    code = r.json()["auth_code"]
    t = requests.post(f"{BASE}/auth/token", data={"grant_type": "authorization_code", "code": code, "client_id": f"{BASE}/"})
    return t.json()["access_token"]

async def setup_totp(token):
    async with websockets.connect(BASE.replace("http", "ws") + "/api/websocket") as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": token}))
        await ws.recv()
        await ws.send(json.dumps({"id": 1, "type": "auth/setup_mfa", "mfa_module_id": "totp"}))
        r = json.loads(await ws.recv())["result"]
        secret = r["description_placeholders"]["code"]
        await ws.send(json.dumps({"id": 2, "type": "auth/setup_mfa", "flow_id": r["flow_id"],
                                  "user_input": {"code": pyotp.TOTP(secret).now()}}))
        assert json.loads(await ws.recv())["result"]["type"] == "create_entry"
        return secret

p = subprocess.Popen([PROG, BASE], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
def run(cmd):
    p.stdin.write(cmd + "\n"); p.stdin.flush()
    return p.stdout.readline().strip()

token = onboard()
check("wrong password", run("password sam wrong"), "BAD_CREDENTIALS")
check("unknown user", run("password nobody correcthorse"), "BAD_CREDENTIALS")
check("right password", run("password sam correcthorse"), "OK")
check("tokens", run("tokens"), "refresh yes, access yes")
check("refresh", run("refresh"), "refresh 1")
check("revoke, then refresh", (run("revoke"), run("refresh"))[1], "refresh -1")
secret = asyncio.run(setup_totp(token))
check("two-factor on: password", run("password sam correcthorse"), "NEED_MFA")
check("wrong code", run("mfa 000000"), "BAD_MFA")
check("right code", run("mfa " + pyotp.TOTP(secret).now()), "OK")
check("refresh after two-factor", run("refresh"), "refresh 1")
print("all ok" if not fails else f"{fails} failed")
sys.exit(1 if fails else 0)

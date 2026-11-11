"""Holds the WebSocket to Home Assistant for ha_live: each message HA sends
goes to the program's stdin as one line; its "SEND <json>" lines go to HA."""
import asyncio, os, sys
import websockets

async def main():
    url, prog = sys.argv[1], sys.argv[2:]
    p = await asyncio.create_subprocess_exec(*prog, stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE)
    async with websockets.connect(url, max_size=None) as ws:
        async def from_ha():
            async for m in ws:
                p.stdin.write(m.replace("\n", " ").encode() + b"\n")
                await p.stdin.drain()
        async def to_ha():
            while True:
                line = await p.stdout.readline()
                if not line:
                    return
                if line.startswith(b"SEND "):
                    await ws.send(line[5:].decode().rstrip("\n"))
        t = asyncio.create_task(from_ha())
        await to_ha()
        t.cancel()
    sys.exit(await p.wait())

asyncio.run(main())

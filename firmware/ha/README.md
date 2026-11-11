# Home Assistant client (portable core)

What the remote's Home Assistant client does, without ESP-IDF in it, so it builds on a PC and is tested against a real Home Assistant. The remote links it from [esp/main/ha_client.c](../esp/main/ha_client.c), which adds the WebSocket and HTTP clients, mDNS, NVS and the cache file. The emulator doesn't use it: it links its demo house, [sim/ha_mock.c](../sim/ha_mock.c), with the same interface ([app/ha.h](../app/ha.h)).

| File | What |
|---|---|
| `ha_json.c` | A zero-copy JSON reader. HA's messages run to hundreds of KB (all states, the registries), so they're walked in place, one array element or object member at a time |
| `ha_store.c` | Areas and entities as `ha.h` shows them: each domain's state and attributes turned into the card fields, service calls, the forecast and to-do items, the cache |
| `ha_ws.c` | The WebSocket conversation: auth, the area, device and entity registries, `subscribe_entities`, service calls, `fire_event` |
| `ha_auth.c` | Sign-in (the login flow, two-factor codes) and the refresh and access tokens |

## Tests

```sh
# against a live Home Assistant, read-only (a long-lived access token in a file only you can read):
firmware/ha/test/run.sh http://192.168.0.12:8123 ~/.config/bleep-ha-token
# and switching one entity off and back on, checking HA's state follows:
firmware/ha/test/run.sh http://192.168.0.12:8123 ~/.config/bleep-ha-token --toggle light.floor_lamp
```

`ha_live.c` prints what the remote would show for one entity of each domain, waits for the weather forecast, and round-trips the cache. Sign-in is tested against a throwaway Home Assistant in Docker (never a real one with a password): start `ghcr.io/home-assistant/home-assistant` on `127.0.0.1:18123`, then build `test/auth_test.c` and run `test/auth_test.py` the way `run.sh` does (it onboards a user, then checks wrong and right passwords, refresh, revoke and two-factor codes).

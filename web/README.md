# Two-player browser Umoria (experimental)

This branch compiles the repository's original Umoria 5.7.15 C++ engine for two
browser clients. It is not a JavaScript imitation of Moria. The original creature,
item, race, class, spell, shop, generation, combat, and inventory code is reused.
The original terminal build remains available through its existing CMake target.

## Run locally

Requirements: Linux, a C++14 compiler (`g++`), `make`, and Python 3.10 or newer.
No npm packages, Python packages, paid APIs, or external services are required.

```sh
./web/build.sh
python3 web/server.py
```

Open **http://localhost:8080**, choose **Create a dungeon**, and copy the invitation
link to the other player. Use separate browser tabs/contexts for two players.
Each tab keeps its own private player token in session storage; the invitation
contains only the room code. Refreshing the original tab reconnects to that player.
A third player cannot claim a slot or control a character without its token.

For another computer on your LAN, start with `HOST=0.0.0.0 python3 web/server.py`,
then use the host computer's LAN address in the browser. Public internet play needs
a host and an HTTPS reverse proxy. No hosting service has been provisioned by this
change. Do not run this as root.

## Container

```sh
docker build -t umoria-coop .
docker run --rm -p 8080:8080 -v moria-saves:/app/saves umoria-coop
```

`SAVE_DIR` defaults to `./saves` locally and `/app/saves` in the container. Back up
that directory (including SQLite WAL files while running) or stop the server
before copying `rooms.sqlite3`. Hosting must attach a persistent writable volume
at `SAVE_DIR`; an ephemeral deployment filesystem does not survive redeployment.

`PORT` defaults to 8080. `MAX_ROOMS` defaults to 8, limiting engine processes.
The image runs as an unprivileged user. The supplied HTTP server is a small
self-hosted experimental server, not a hardened public multi-tenant service.

## Controls

Click the terminal to focus it. Follow original character creation: choose a
race, choose sex, Space to reroll / Escape to accept, choose class, type name,
and Enter. Use arrows or numeric keypad 1–9 to move, 5 to wait, `?` for original
help, `i` inventory, `e` equipment, `w` wield, `<`/`>` stairs, and Escape to close
a menu. Original Moria commands are used, not WASD. `@` is you; `&` is your
companion. Native ASCII art and the 80×24 layout are retained.

## Multiplayer rules and deliberate differences

- Two independent characters share one authoritative dungeon, its monsters,
  ground items, town shops, exploration memory, and item identification knowledge.
- Both players submit commands without waiting for the other. The server processes
  actions serially, preserving a single source of truth. Monsters respond to each
  completed world action and select the nearest living player. There is no free
  running monster clock while both players are idle. This changes multiplayer
  pacing and is not frame-based real-time combat.
- Rest, running, repetition, and paralysis advance at bounded scheduler ticks.
- Stairs, recall, and trap-door level transitions take the party together.
  A companion's pending prompt is canceled when they next provide input after a
  level transition. Players cannot occupy different dungeon depths.
- Inventories, equipment, HP, gold, stats, character creation, and input screens
  are separate. Characters cannot occupy the same tile. Ground items are consumed
  once; drop items for a companion to collect. No PvP attacks are added.
- One player uses a given shop at a time, so a suspended haggle cannot refer to an
  item the other player has already bought. Other shops remain available.
- Long messages automatically continue; the original message history remains.
- **Automatic durable saves:** each accepted input and automatic turn is committed
  to a SQLite action journal before the response is acknowledged. The seeded
  engine and recorded action order reconstruct both players, including open
  menus, after a restart. Idle rooms suspend after 30 minutes; their saves remain.
  Recovery requires the same engine binary. Version mismatches preserve the save
  and refuse to load it rather than silently changing a run. Long runs take longer
  to replay because this initial implementation has no compact checkpoints.
- Use **Copy my resume link** to return from a new browser or tab. Keep it private:
  it contains the capability to control your character. Invitation links never
  contain that token. The resume token stays in the URL fragment, is removed from
  the address bar on arrival, and is stored in the tab session.
- Legacy single-player save/score formats are not used. Ctrl-X explains browser
  reconnection. File export and wizard mode are disabled. Options are room-wide.
- Native spell behavior is retained where possible; terrain effects avoid burying
  the companion. Monster breath damage is routed to each affected player.

This is a playable experimental adaptation, **not a claim of complete behavioral
parity** with every original single-player edge case. Full-depth Balrog victory,
every spell/item interaction, and long-running production load still need playtesting.
Do not merge it into a release branch as a verified complete clone.

## Verification

```sh
./web/build.sh
make -f web/engine.mk build-web/engine-tests
(cd build-web && ./engine-tests)
python3 tests/test_server.py
python3 tests/test_replay.py
```

The engine tests execute original commands through interleaved player stacks:
character creation in either completion order, shared world, movement, collision,
independent inventories, menu concurrency, dropped-item pickup, monster targeting,
shop ownership, level change with a pending menu, and independent death.
HTTP tests run two simultaneous clients and check room capacity, private session
tokens, invalid input rejection, cross-origin rejection, and exact recovery after
killing/restarting the server during a menu. Recovery tests cover automatic turns,
a simulated failed durable write, and preserving incompatible-version saves.

## Implementation

`MORIA_COOP` selects a memory-backed terminal and stable per-player records.
Linux `ucontext` gives each player an independent call stack, allowing original
multi-step prompts to suspend without holding up the other player. A room engine
runs on one thread; Python serializes its input protocol. World state stays shared.
A level-generation epoch invalidates old prompts before they can modify a new map.
Store guards prevent stale references during concurrent purchases. Tests have
access to deterministic fixtures; production HTTP clients do not.

All original copyright notices and GPL-3.0-or-later licensing are retained.

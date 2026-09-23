# Moria Online — The Shared Deep

A browser-playable, cooperative Moria-inspired dungeon crawler. This is a new TypeScript multiplayer implementation, not a compiled port or a complete recreation of Umoria's original rules. The original Umoria desktop source is preserved in the parent repository.

## Play

Choose Warrior, Mage, or Ranger, name your adventurer, and enter. Walk north from the town spawn to the `>` stairs, then press E. Explore five connected dungeon depths and defeat the Balrog at 500 feet. Players share the same map, monsters, loot, and chat. Nearby allies share experience.

- WASD / arrows: move; walking into a monster attacks.
- Space / 1: attack; Rangers shoot up to four tiles.
- 2: arcane bolt (5 mana, six-tile range).
- Q / 3: healing potion.
- E / 4: use stairs underfoot.
- R: recall to town for 10 gold.
- Enter: chat. `?`: guide.
- Touch controls are provided on narrow screens.

Town heals players. Monsters and treasure respawn. Death returns you to town, losing 20% of carried gold. Upgrade weapons and armor or buy potions from the town trader. Save & leave disconnects safely.

## Multiplayer and persistence

The server validates movement, cooldowns, damage, purchases, and rewards. A persistent D1 realm document uses revision-checked compare-and-swap writes and bounded retries to prevent lost updates and duplicate rewards. Clients poll every 500 ms while visible; movement may be sent every 170 ms. Simulation advances on incoming requests, capped at one monster tick per 650 ms and one regeneration tick per 2500 ms. There is no background simulation when nobody sends requests.

Characters persist server-side and are recovered with a random HttpOnly, SameSite cookie (Secure in HTTPS). Cookie tokens are hashed before use as database keys and never exposed in snapshots. Clearing cookies loses access to that character. Separate browsers, private sessions, or devices create independent players. This build has no account recovery or cross-device login.

This is an early cooperative multiplayer realm, not a production-scale MMO: 64 active-player admission cap, 1,000 saved-character cap, no sharding, and no full-capacity load-test claim. The single D1 document and polling transport are deliberate small-realm limits. Presence expires after 15 seconds; disconnected characters are no longer attack targets. Player-vs-player damage is disabled. Public names and chat are user supplied; this build has no moderation console.

## Development

Node 22.13+ and pnpm (see packageManager in package.json).

```sh
pnpm install --frozen-lockfile
pnpm build
node --import ./scripts/sites-env.mjs ./node_modules/wrangler/bin/wrangler.js d1 execute DB --local --config dist/server/wrangler.json --persist-to .wrangler/state --file drizzle/0000_greedy_vapor.sql
pnpm dev
```

Apply the migration only once per local database. Production hosting provisions the logical DB binding and applies the committed Drizzle migration. No external billing credentials or paid services are configured in this code. Hosted costs and quotas depend on the hosting account.

## Verification

```sh
node --experimental-strip-types --test tests/game.test.mjs
node --test tests/server.test.mjs
pnpm exec tsc --noEmit
pnpm build
```

Engine tests cover floor reachability, two-player presence and shared rewards, fog-of-war snapshots, invalid movement, duplicate actions, death, purchases, stairs, resume, and safe logout. The server test runs the actual route on a local Cloudflare Worker/D1 emulator with two independent cookie sessions and concurrent requests, including overspend prevention. No browser or network access is required for that test. Optional `tests/api-smoke.mjs` runs the same checks against an explicitly configured `MORIA_TEST_URL`; use a disposable test environment, since it creates characters.

Browser preview checked: character creation, keyboard movement, map rendering, and guide. WebMCP inspection support is feature-detected; the preview browser did not expose modelContext, so that optional integration was not runtime-validated.

## Credits and licensing

Inspired by The Dungeons of Moria by Robert Alan Koeneke and Umoria by James E. Wilson and contributors. This browser implementation is offered under GPL-3.0-or-later; see LICENSE. Third-party framework and UI dependencies retain their own licenses. The vendored Sites Vite plugin includes its MIT notice in build/sites-vite-plugin.LICENSE. No proprietary game artwork is used; the dungeon uses classic glyphs and functional tile geometry.

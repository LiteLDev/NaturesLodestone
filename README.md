# NaturesLodestone

[![Build](https://github.com/LiteLDev/NaturesLodestone/actions/workflows/build.yml/badge.svg)](https://github.com/LiteLDev/NaturesLodestone/actions/workflows/build.yml)
[![License](https://img.shields.io/github/license/LiteLDev/NaturesLodestone)](COPYING.LESSER)

Find any biome with a compass. NaturesLodestone brings Chaosyr's Java mod [Nature's Compass](https://modrinth.com/mod/natures-compass) to Bedrock, for dedicated servers and single-player worlds.

> [!NOTE]
> NaturesLodestone is in early development. There is no release yet.

## Using the compass

**Craft it** on a crafting table: a compass in the middle, logs on the four sides and four saplings of one kind in the corners.

| What you do | What happens |
| --- | --- |
| Right-click (in the air or on a block) | Opens **Select Biome**. |
| Sneak + right-click | Stops the search and resets the compass. Works on a broken compass too. |
| Hold it | The HUD in the top-left corner shows the compass's status, and the needle points to the biome it found. Before a search, the needle points to the world spawn. |

**Select Biome** lists every biome the world can generate. Each row shows the biome's dimension, tags, source and XP cost.

- **Search box.** Filters by name. `$forest` filters by tag, `@minecraft` by source.
- **Sort By** cycles through Name, Source, Tags, Dimension, Levels, Rainfall and Temperature.
- **Choose a biome.** Click it, then **Search**, or click it again. A search costs the biome's XP levels (nothing in creative). Biomes you can't afford are greyed out.
- **Search for Next** finds another patch of the biome you found, one that isn't connected to any earlier find. Turn it off with `maxNextSearches: 0`.
- **Teleport** takes you to the biome you found, at a safe height. Only operators, or players in a world with cheats on, can use it, and only while `allowTeleport` is on. If the chunks there aren't loaded yet, you wait above them until they are (at most 20 s).
- **Cancel** closes the screen.

The search runs on the server, a little every tick, spiralling out from where you stand. While it runs, the HUD shows **Searching** and the radius. When it ends, the HUD shows **Found** with the coordinates and your distance (with `displayCoordinates`), or **Not Found** with the radius and the number of samples.

With `compassDurability` set, every successful search (outside creative) wears the compass down, and the durability bar shows how much is left. A broken compass can't search. Repair it on a crafting table: Nature's Compass + compass gives you a fresh one.

### Servers, clients and vanilla players

| Where | Select Biome | HUD | Needle |
| --- | --- | --- | --- |
| Single-player (client mod) | yes | drawn by the mod | animated |
| Dedicated server, player with the client mod | yes | drawn by their mod | animated |
| Dedicated server, vanilla player | yes | drawn by the server's resource pack | static icon |

## Install

You need Minecraft Bedrock Edition **1.26.51** for Windows (x64) with LeviLamina **26.51.x** (on a client, installed through [LeviLauncher](https://github.com/LiteLDev/LeviLauncher)).

With [lip](https://github.com/LiteLDev/lip):

```sh
lip install github.com/LiteLDev/NaturesLodestone           # dedicated server
lip install github.com/LiteLDev/NaturesLodestone#client    # client
```

Or download the zip from the [latest release](https://github.com/LiteLDev/NaturesLodestone/releases/latest) and extract it next to `LeviLamina/`:

| Where | Zip | Folder |
| --- | --- | --- |
| Dedicated server | `natureslodestone-server-windows-x64.zip` | `plugins/NaturesLodestone/` |
| Client (LeviLauncher) | `natureslodestone-client-windows-x64.zip` | `mods/NaturesLodestone/` |

## Commands

| Command | Who | What it does |
| --- | --- | --- |
| `/naturescompass reload` | Operators, console | Reloads the configuration and translations. |
| `/naturescompass selftest ["glob"]` | Operators, console | Runs the in-game selftests. |

`/natureslodestone` is an alias of `/naturescompass`. Nature's Compass has no commands. The other sub-commands (`damage`, `gui`, `hud`, `needle`) are debug tools for the tests. They are listed in [docs/SPEC.md](docs/SPEC.md#6-commands).

## Configuration

Settings live in `config/*.json` inside the mod folder (`plugins/NaturesLodestone/` or `mods/NaturesLodestone/`). Each file is created with defaults on first start.

| File | Setting | Default | Meaning |
| --- | --- | --- | --- |
| `mod.json` | `language` | `en_US` | Language of server messages when a player's own language has no translation. |
| | `usePlayerLocale` | `true` | Use each player's game language when a translation exists. |
| | `requireResourcePack` | `true` | Dedicated server: players must accept the resource pack to join. |
| `naturescompass.json` | `allowTeleport` | `true` | Operators and players in worlds with cheats can teleport to a located biome. |
| | `maxNextSearches` | `25` | 0 to 10000. 0 hides "Search for Next", so the compass always locates the nearest biome. |
| | `displayCoordinates` | `true` | Show the coordinates and distance of a located biome on the HUD. |
| | `radiusModifier` | `2500` | 0 to 1000000. The maximum search radius is 4 × this, in blocks. |
| | `sampleSpaceModifier` | `16` | 0 to 1000000. The distance between two samples is 4 × this, in blocks. Lower is more accurate and slower. |
| | `biomeBlacklist` | `[]` | Biome ids that can't be searched for. `*` matches any characters, `?` one character, e.g. `"minecraft:*ocean*"`. |
| | `defaultXpLevels` | `0` | 0 to 3. XP levels a search costs. |
| | `perBiomeXpLevels` | `[]` | `"biome_id,levels"` entries that override `defaultXpLevels` (wildcards allowed, at most 3 levels), e.g. `"minecraft:deep_dark,3"`. |
| | `maxSamples` | `50000` | 0 to 1000000. Maximum samples one search takes. |
| | `compassDurability` | `0` | 0 to 10000. Successful searches before the compass breaks and must be repaired. 0 disables durability. |
| `naturescompass_client.json` | `displayWithChatOpen` | `true` | Show the HUD lines while the chat is open. |
| | `fixBiomeNames` | `true` | Add missing spaces to biome names in the list ("ForestHills" becomes "Forest Hills"). |
| | `overlayLineOffset` | `1` | 0 to 50. Line offset of the HUD lines. |
| | `overlaySide` | `LEFT` | `LEFT` or `RIGHT`: the side of the screen for the HUD lines. |

`naturescompass.json` is the Nature's Compass "General" section. `naturescompass_client.json` is its "Client" section. In single-player, the client mod reads both. On a dedicated server, the server's `naturescompass_client.json` controls the HUD it sends to vanilla players (and the biome name fixing in Select Biome). Players with the client mod use their own copy for their HUD. `/naturescompass reload` applies changes.

## Differences from Nature's Compass

NaturesLodestone matches Nature's Compass 3.3.1 everywhere except where Bedrock forces a difference. The full list is in [docs/SPEC.md](docs/SPEC.md) (sections 1.3 and 8).

- **Select Biome is a form.** The server sends it, so vanilla players can use it too. With the resource pack (single-player, and servers with `requireResourcePack`) it is drawn in NC's layout. The search box opens a small input and filters when you confirm it, not on every key. Players without the pack get a plain form with the same contents.
- **Vanilla players on a server.** The HUD is sent as a title that the resource pack draws in the same place. It can't hide itself while the chat is open, and it can replace a normal title that is on screen when the HUD changes. The needle is a static icon.
- **One search per compass.** In NC, one player's search cancels another's.
- **Saplings.** All four saplings in the recipe must be of the same kind.
- **Teleporting into unloaded chunks** waits until they have loaded.
- **Durability** shows as Bedrock's durability bar.
- **Biome ids** are Bedrock's (for example `minecraft:roofed_forest` is Dark Forest). Names and tags follow Java.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). [AGENTS.md](AGENTS.md) is the developer guide.

## Credits

NaturesLodestone reproduces the behaviour of [Nature's Compass](https://github.com/MattCzyr/NaturesCompass) by Chaosyr. It copies no code or assets from it.

## License

[LGPL-3.0-or-later](COPYING.LESSER)

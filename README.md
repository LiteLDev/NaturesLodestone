# NaturesLodestone

[![Build](https://github.com/LiteLDev/NaturesLodestone/actions/workflows/build.yml/badge.svg)](https://github.com/LiteLDev/NaturesLodestone/actions/workflows/build.yml)
[![License](https://img.shields.io/github/license/LiteLDev/NaturesLodestone)](COPYING.LESSER)

Find any biome with a compass. NaturesLodestone brings Chaosyr's Java mod [Nature's Compass](https://modrinth.com/mod/natures-compass) to Bedrock, for dedicated servers and single-player worlds.

> [!NOTE]
> NaturesLodestone is in early development. There is no release yet.

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

`/natureslodestone` is an alias of `/naturescompass`.

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

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). [AGENTS.md](AGENTS.md) is the developer guide.

## Credits

NaturesLodestone reproduces the behaviour of [Nature's Compass](https://github.com/MattCzyr/NaturesCompass) by Chaosyr. It copies no code or assets from it.

## License

[LGPL-3.0-or-later](COPYING.LESSER)

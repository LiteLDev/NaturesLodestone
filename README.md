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

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). [AGENTS.md](AGENTS.md) is the developer guide.

## Credits

NaturesLodestone reproduces the behaviour of [Nature's Compass](https://github.com/MattCzyr/NaturesCompass) by Chaosyr. It copies no code or assets from it.

## License

[LGPL-3.0-or-later](COPYING.LESSER)

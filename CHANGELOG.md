# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- The Nature's Compass item (`natureslodestone:naturescompass`) with its crafting recipe (saplings, logs and a compass) and the repair recipe (Nature's Compass + compass).
- The `naturescompass.json` and `naturescompass_client.json` configuration files with every Nature's Compass setting.
- The behaviour spec of the port in `docs/SPEC.md`.
- Using the compass: right-click (in the air or on a block) opens Select Biome, sneak + right-click resets the compass; one action per click.
- Search and Search for Next take the configured XP levels (not in creative) and play the click sound; a broken compass can't search.
- Teleport to a found biome (operators or cheats, `allowTeleport`) at a safe height, also into chunks that are not loaded yet.
- The durability bar of the compass when `compassDurability` is set.

[Unreleased]: https://github.com/LiteLDev/NaturesLodestone/commits/main

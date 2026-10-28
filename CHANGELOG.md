# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- The Nature's Compass item (`natureslodestone:naturescompass`) with its crafting recipe (saplings, logs and a compass) and the repair recipe (Nature's Compass + compass).
- The `naturescompass.json` and `naturescompass_client.json` configuration files with every Nature's Compass setting.
- The behaviour spec of the port in `docs/SPEC.md`.
- The animated compass needle (client build): 32 needle frames of original pixel art that point at the found biome, or at the world spawn before a search, turning with the player like a vanilla compass and spinning when dropped. Vanilla clients see the plain icon.
- The biome catalog: every biome the world can generate, with its dimensions, Nature's Compass tags, source, temperature, rainfall and XP cost; the biome blacklist and per-biome XP costs; English and Chinese names for every biome and dimension.
- Biome searches as in Nature's Compass: a square spiral over the world generator's biomes (no chunk loading) in the Overworld, the Nether and the End, within a per-tick time budget, with radius progress, Not Found limits and "Search for Next" skipping biome patches connected to earlier finds. One search per compass.

[Unreleased]: https://github.com/LiteLDev/NaturesLodestone/commits/main

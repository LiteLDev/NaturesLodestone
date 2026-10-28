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
- The biome catalog: every biome the world can generate, with its dimensions, Nature's Compass tags, source, temperature, rainfall and XP cost; the biome blacklist and per-biome XP costs; English and Chinese names for every biome and dimension.
- Biome searches as in Nature's Compass: a square spiral over the world generator's biomes (no chunk loading) in the Overworld, the Nether and the End, within a per-tick time budget, with radius progress, Not Found limits and "Search for Next" skipping biome patches connected to earlier finds. One search per compass.
- The Nature's Compass HUD: status, biome, radius, coordinates, distance and samples of the held compass, drawn natively by the client mod and sent by the server to vanilla clients (rendered by the resource pack), on the left or right side (`overlaySide`, `overlayLineOffset`, `displayWithChatOpen`).
- The Select Biome screen: search box (`$tags`, `@source` filters), Sort By (Name, Source, Tags, Dimension, Levels, Rainfall, Temperature), biome rows with dimension / tags / source lines and XP level costs, greyed rows the player can't afford, selection (a second click searches), Search, Search for Next, Teleport and Cancel. It is a data-driven form that updates in place, with a classic form fallback drawn in Nature's Compass' layout by the resource pack.

[Unreleased]: https://github.com/LiteLDev/NaturesLodestone/commits/main

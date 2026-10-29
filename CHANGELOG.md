# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-10-29

First release: Nature's Compass 3.3.1 for Bedrock 1.26.51 (LeviLamina 26.51), on dedicated servers and in single-player.

### Added

- The Nature's Compass item (`natureslodestone:naturescompass`) with its crafting recipe (saplings, logs and a compass) and the repair recipe (Nature's Compass + compass).
- Using the compass: right-click (in the air or on a block) opens Select Biome, sneak + right-click resets the compass; one action per click.
- The Select Biome screen: search box (`$tags`, `@source` filters), Sort By (Name, Source, Tags, Dimension, Levels, Rainfall, Temperature), biome rows with dimension / tags / source lines and XP level costs, greyed rows the player can't afford, selection (a second click searches), Search, Search for Next, Teleport and Cancel. It is drawn in Nature's Compass' layout by the resource pack; players without the pack get a data-driven form with the same contents.
- The biome catalog: every biome the world can generate, with its dimensions, Nature's Compass tags, source, temperature, rainfall and XP cost; the biome blacklist and per-biome XP costs; English and Chinese names for every biome and dimension.
- Biome searches as in Nature's Compass: a square spiral over the world generator's biomes (no chunk loading) in the Overworld, the Nether and the End, within a per-tick time budget, with radius progress, Not Found limits and "Search for Next" skipping biome patches connected to earlier finds. One search per compass.
- Search and Search for Next take the configured XP levels (not in creative) and play the click sound; a broken compass can't search.
- Teleport to a found biome (operators or cheats, `allowTeleport`) at a safe height, also into chunks that are not loaded yet.
- Durability (`compassDurability`): successful searches wear the compass down, shown on its durability bar.
- The Nature's Compass HUD: status, biome, radius, coordinates, distance and samples of the held compass, drawn natively by the client mod and sent by the server to vanilla clients (rendered by the resource pack), on the left or right side (`overlaySide`, `overlayLineOffset`, `displayWithChatOpen`).
- The animated compass needle (client build): 32 needle frames of original pixel art that point at the found biome, or at the world spawn before a search, turning with the player like a vanilla compass and spinning when dropped. Vanilla clients see the plain icon.
- The `naturescompass.json` and `naturescompass_client.json` configuration files with every Nature's Compass setting.
- The behaviour spec of the port in `docs/SPEC.md`, and usage, configuration and the differences from Nature's Compass in the README.

[Unreleased]: https://github.com/LiteLDev/NaturesLodestone/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LiteLDev/NaturesLodestone/releases/tag/v0.1.0

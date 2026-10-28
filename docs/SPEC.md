# NaturesLodestone behaviour spec

What NaturesLodestone reproduces from the Java mod [Nature's Compass](https://modrinth.com/mod/natures-compass) **3.3.1** (Chaosyr).
This is the behaviour reference for every feature unit (see [AGENTS.md](../AGENTS.md)). It describes
behaviour only: never copy code or assets from Nature's Compass (NC below).

Wording: "NC does X" is the Java behaviour; "we" is the Bedrock port. Where the two differ, the
difference is a **decided deviation** and listed as such (section 1.3) or under "Not ported"
(section 8). Everything else must match NC exactly.

## 1. Overview

Nature's Compass is one item. Right-clicking it opens a **Select Biome** screen listing every
biome of the world. Choosing one starts a search that runs on the server over several ticks,
sampling the world generator's biome source outward from the player in a square spiral (no
chunk is loaded). While searching and afterwards the held compass shows its status on the HUD
(status, biome, radius, coordinates, distance, samples) and its needle points at the found
position. A found biome can be searched again for its *next* instance, and players with the
permission can teleport to it. Sneak-using the compass resets it.

### 1.1 Platforms

One code base, two builds:

| Build | Runs in | Who is served |
| --- | --- | --- |
| server | BDS (`plugins/NaturesLodestone`) | every player on the server: vanilla clients and clients running the client build |
| client | the game client (`mods/NaturesLodestone`) | the local player in local worlds (the integrated server runs the same server-side code), and the local player's own rendering (HUD, needle) when joining a BDS that runs the server build |

All gameplay (item use, searches, data, teleport, XP, durability) is server side. The client
build adds native rendering for its local player (HUD overlay, animated needle).

### 1.2 Ids

| NC | NaturesLodestone |
| --- | --- |
| `naturescompass:naturescompass` | `natureslodestone:naturescompass` |
| recipe `naturescompass:natures_compass` | `natureslodestone:natures_compass` (+ one per sapling type, see 2.2) |
| recipe `naturescompass:repair_natures_compass` | `natureslodestone:repair_natures_compass` |
| lang `item.naturescompass.naturescompass` | RP texts `item.natureslodestone:naturescompass.name` |
| lang `string.naturescompass.*` | the same keys, in `assets/lang/<locale>/compass.json` |

### 1.3 Decided deviations

1. **Select Biome is a server-driven form.** NC's screen is client code fed by a sync packet. We
   send a form from the server (an `ll::ui` data-driven `CustomForm` preferred, a classic
   `ll::form` form as fallback), so it works in local worlds, for modded clients on BDS and for
   vanilla clients on BDS. Layout content, strings, sort, filter, selection, greying, XP costs
   and the pre-selection of the found biome follow NC (section 4.2). An RP JSON-UI skin
   (`ui/nl_gui*.json`, registered through `ui_defs.d/gui.json`) restyles the form toward NC's look
   (dark translucent buttons, two-column layout).
2. **HUD.** The client build draws NC's HUD natively for its local player with NC's exact layout.
   Every other player (vanilla clients on BDS) gets the same line block as a server-sent, marked
   title/actionbar string that an RP `hud_screen.json` modification renders as the same block at
   the same place (section 4.3).
3. **Needle.** Animated through a client hook (client build; also when the modded client joins a
   BDS). Vanilla clients see a static icon (section 8).
4. **One search worker per compass.** NC keeps one worker on the item object, so one player's
   search cancels another's (a bug). We run one worker per compass, keyed by the compass `uid`,
   and keep NC's per-tick time budget and its first-come-first-served order.
5. **Config sections.** `config/naturescompass.json` holds every NC "General" key,
   `config/naturescompass_client.json` the NC "Client" keys. In local worlds the client build
   reads the client section. On BDS the server's copy of the client section drives the fallback
   HUD it sends.
6. **Durability.** Shown as the Bedrock durability bar scaled to `compassDurability`; no bar when
   `compassDurability` is 0 (section 3.7).
7. **Teleport permission.** `allowTeleport && (operator || world cheats enabled)`. Operator =
   LeviLamina `isOperator()` or command permission level >= GameDirectors. Creative alone is not
   enough (as in NC).
8. **Recipe saplings.** Bedrock recipes cannot use a "#saplings" tag; there is one recipe per
   sapling type, so the four saplings of one craft must be of the same type (section 2.2).

## 2. Blocks and items

No blocks.

### 2.1 Nature's Compass (`natureslodestone:naturescompass`)

| Property | Value |
| --- | --- |
| Name | "Nature's Compass" / "自然罗盘" (RP `texts.d/<locale>/compass.lang`) |
| Stack size | 1 |
| Creative inventory | Equipment, group `minecraft:itemGroup.name.compass` (next to the compasses) |
| Tags | `minecraft:compasses` |
| Off hand | allowed (`minecraft:allow_off_hand`) |
| Icon | texture key `naturescompass` (defined by `item_texture.d/needle.json`, owned by U7) |
| Durability | see 3.7 |

Behavior pack: `assets/pack/bp/items/naturescompass.json` (owned by U1; U4 may add the
durability component, see 3.7).

### 2.2 Recipes

| Id | Type | Pattern / ingredients | Unlock |
| --- | --- | --- | --- |
| `natureslodestone:natures_compass` | shaped, crafting table | `SLS` / `LCL` / `SLS`; S = oak sapling, L = tag `minecraft:logs`, C = `minecraft:compass` | has `minecraft:compass` |
| `natureslodestone:natures_compass_<sapling>` | the same | S = `spruce_sapling`, `birch_sapling`, `jungle_sapling`, `acacia_sapling`, `dark_oak_sapling`, `pale_oak_sapling`, `cherry_sapling`, `mangrove_propagule`, `azalea`, `flowering_azalea` | has `minecraft:compass` |
| `natureslodestone:repair_natures_compass` | shapeless, crafting table | Nature's Compass + `minecraft:compass` | has `natureslodestone:naturescompass` |

The repair recipe outputs a fresh compass: damage and all search data are gone (the result has
no user data). It is always registered (also when `compassDurability` is 0). NC's recipe group
"tools" has no Bedrock equivalent and is dropped.

### 2.3 Item data

Stored on the `ItemStack`'s user data, a compound under the key `nl:compass`
(`src/feature/compass/CompassData.h`). Missing keys read as defaults (Inactive). The data is in
the normal item NBT, so it is saved with the item everywhere and synced to clients.

| Key | NBT | Default | Meaning (NC name) |
| --- | --- | --- | --- |
| `compass_state` | Int | 0 | 0 Inactive, 1 Searching, 2 Found, 3 Not Found (`compass_state`) |
| `biome_id` | String | "" | `minecraft:plains` (`biome_id`) |
| `found_x`, `found_z` | Int | 0 | x/z of the matching sample, no y (`found_x`, `found_z`) |
| `search_radius` | Int | 0 | progress / final radius floored to a multiple of 500 (`search_radius`) |
| `samples` | Int | 0 | samples taken (`samples`) |
| `display_coords` | Byte | 0 | copy of `displayCoordinates` at success time (`display_coords`) |
| `prev_pos` | List of {x,y,z} Int compounds | [] | every position found so far, with its matching y (`prev_pos`) |
| `damage` | Int | 0 | successful searches (`damage`) |
| `uid` | Int64 | 0 | ours: a random non-zero id given at the first search, to find the compass instance again |

Writes ("clear" = every search key reset, `damage` and `uid` kept):

| Event | Data afterwards |
| --- | --- |
| search start (also Search for Next) | clear; Searching, `biome_id`, `samples` 0, `search_radius` 0 |
| progress | `search_radius` = floor(radius / 500) * 500 whenever radius > 500 and that value grew |
| success | clear; Found, `biome_id`, `found_x`, `found_z`, `prev_pos` (with the new position appended), `samples`, `display_coords`; `damage` + 1 unless the player is in creative |
| failure | clear; Not Found, `biome_id`, `search_radius` (floored), `samples` |
| sneak-use | clear (Inactive) |

## 3. Mechanics

### 3.1 Holding the compass

Every action uses the compass in the **main hand first, then the off hand**. Only a held compass
matters: the GUI, every action, the HUD and the needle look at the held stack only.

### 3.2 Using the compass (right click)

- **Not sneaking.** Broken compass (3.7): nothing happens (NC PASS). Otherwise the Select Biome
  screen opens (NC opens the client GUI and sends the sync packet; we send the form, which reads
  the same data: biome list, `canTeleport`, `maxNextSearches`, infinite XP).
- **Sneaking.** The compass's running search stops (no write), its search data is cleared
  (Inactive). Works on a broken compass too. NC suppresses the re-equip animation when the state
  did not change; on Bedrock the inventory update causes no re-equip animation anyway.
- Use applies to right-clicking in the air and on a block (NC `use` is reached for both when the
  block does nothing).
- We (U4): both the air and the block use event count; a block click is left to the block when the
  player does not sneak and the block is interactive. Events closer than 6 ticks to the previous
  one (held button, block + air event of one click) belong to the same click: one action per
  click. The action runs on the next tick (item writes inside the game's use handling could be
  overwritten). Log lines: `Compass used by <name>: Select Biome`, `Compass reset by <name>`.

### 3.3 Search

Server thread, cooperative: one worker per compass (deviation 1.3.4).

- **Budget.** Each server tick, after the tick's own work: budget = 50 ms - time spent in the
  tick so far, at least 10 ms. Workers run in start order; the first one gets the whole budget
  until it finishes, then the next. The deadline is checked before each `doWork()` (one call may
  overshoot it). One `doWork()` = one sample column or one connection-check step. Server stop /
  world close clears all workers silently (no resume; a compass left Searching stays so until it
  is reset or searches again). We time the server level's tick (`Level::tick`, NC: the whole
  server tick from `ServerTickEvent.Pre`) and run the workers right after it.
- **Parameters.** biomeSize = 4. sampleSpace = sampleSpaceModifier * 4 (default 64 blocks).
  maxRadius = radiusModifier * 4 (default 10000 blocks). maxSamples (default 50000). If maxRadius
  or sampleSpace is 0 the search fails immediately (Not Found, radius 0, 0 samples, no start log
  line).
- **Start position.** Search: the player's block position when the button is pressed (NC trusts
  the client's position; we use the server's). Search for Next: the player's block position.
- **Dimension.** The player's current dimension at the start, sampled from its generator's biome
  source (`BiomeSource::_getBiome`, no chunk loading) for the whole search. The dimension is not
  stored on the compass.
- **Y levels.** NC `Mth.outFromOrigin(startY, minY + 1, maxY, 64)` (bounds inclusive, maxY = the
  top block y): the start y, then alternately above and below it in steps of 64 while within the
  bounds, e.g. 64, 128, 0, 192, 256 (overworld) and only 64 in the Nether (1 .. 127). We clamp a
  start y outside the bounds into them (NC: no level, nothing can match; Bedrock's Nether is only
  128 blocks high, so a player on its roof would otherwise never find anything). Every level is tested per x/z sample in
  that order; the first match gives the candidate's y. Coordinates go to quart resolution (>> 2)
  for the noise biome lookup (we pass the quart's lowest block position). A level matches when
  the biome id equals the searched id.
- **Spiral.** The first sample is the start. Then directions cycle north (-z), east (+x),
  south (+z), west (-x) with segment lengths 1, 2, 3, 4, 5, ... sample spaces. Points (in sample
  spaces): (0,0), (0,-1), (1,-1), (2,-1), (2,0), (2,1), (2,2), (1,2), ...
- **Loop.** While not finished and radius <= maxRadius and samples <= maxSamples, where radius =
  (int) sqrt((float) horizontal distance squared) of the last sampled point from the start. The
  check comes before the move, so one point beyond maxRadius and maxSamples + 1 samples can be
  taken. When the condition fails the search fails with that radius and sample count. After each
  non-matching sample, and after each Search for Next candidate found connected, `samples`++ (an
  accepted match is not counted). `search_radius` progress: after that increment, when radius >
  500 and radius / 500 grew, floor(radius / 500) * 500 is written to the compass (2.3).
  `samples` is written only at the end.
- **Match, normal search** (`prev_pos` empty): success at once; the candidate (x/z of the sample
  with its matching y) is appended to `prev_pos`.
- **Match, Search for Next** (`prev_pos` not empty): the previous positions are checked in order.
  For each one (one `doWork()` to start, no lookup): d = horizontal distance; if d <= 2 *
  sampleSpace the candidate is *connected* (skip it, continue the spiral). Else N = (int)(d /
  sampleSpace); points t = i / N for i = 1 .. N-1 on the straight line previous -> candidate,
  x/z = (int)(prev + delta * t) (truncated toward zero), are tested at all y levels, one point per
  `doWork()`; a matching point resets the count of consecutive non-matching points, and once that
  count reaches clamp(N / 4, 1, 10) (integer division) the candidate is *not connected* to that
  previous position (go on with the next one). If every line point passes without such a gap it
  is connected (skip). A candidate not connected to any previous position is accepted: appended
  to `prev_pos`, success.
- **Success / failure.** Success: the compass data as in 2.3 (`damage` + 1 unless the player has
  infinite materials, `actions::hasInfiniteXp`). Failure: Not Found with the floored radius and
  the samples. The worker finds the compass again by `uid` in its player's inventory (held ones
  first) at every write; when the compass is gone (dropped, stored, on the cursor) the search goes
  on, progress writes are skipped and the final write logs `Invalid compass after search` (a
  warning) instead, so a dropped or stored compass stays Searching. When the player is offline at
  the start of a tick the worker is dropped (`Search stopped` log line, no write) instead of
  holding the budget (NC keeps it running to its end). NC drops a Search for Next worker silently (no
  write, no log) when a skipped connected candidate exhausts the limits; we fail it like any other
  search, so the compass never stays Searching.
- **Stop.** Sneak-use or a new search of the same compass stops the worker: no data written.
- **Log lines** (NC's text; `<id>` = 8 random hex digits per worker):
  `BiomeSearchWorker <id>: Starting search: <sampleSpace> sample space, <maxSamples> max samples, <maxRadius> max radius, <n> previous locations`,
  `BiomeSearchWorker <id>: Search succeeded: <radius> radius, <samples> samples`,
  `BiomeSearchWorker <id>: Search failed: <radius> radius, <samples> samples`,
  `BiomeSearchWorker <id>: Search stopped: <radius> radius, <samples> samples`.
- **Code.** The spiral, y levels and connection check are pure logic in `src/core/Spiral.h`
  (gtests in `tests/core/SpiralTest.cpp`); sampling, the budget and the compass writes are in
  `src/feature/search/`.

### 3.4 Actions (server side of NC's packets)

| Action | Rules |
| --- | --- |
| Search (biome id) | uses the held compass; ignored if broken. Takes the biome's XP levels (0..3) unless creative, clamped at 0 (no server-side "enough levels" check, as NC; the screen greys entries the player can't afford). Starts a search (3.3). The biome id is not validated against the list (as NC). |
| Search for Next | ignored if broken, if maxNextSearches is 0, or unless the compass is Found (needs `biome_id` and `prev_pos`). XP cost of that biome as above. maxNextSearches only controls the button; there is no counting. |
| Teleport | needs `canTeleport` (3.5) and state Found; target found_x / found_z in the player's current dimension. |

### 3.5 Teleport

- Permission: `allowTeleport && (operator || world cheats enabled)`. Denied:
  log `Player <name> tried to teleport but does not have permission.` and nothing happens.
- Height: start at sea level (y 63 in the overworld); each step probe one block up and one block
  down from the start (y+k, y-k) while either probe is within the build height and neither is
  valid. Valid = feet and head blocks "free" and the block below not free. Free = air, fire,
  liquid, or replaceable. The up probe wins over the down probe. None found: y = 256.
- The player stops riding, is teleported to (x, y, z) as whole block coordinates (not centred),
  keeping yaw and pitch. Unless gliding with an elytra, vertical velocity is set to 0 and the
  player is on the ground.
- After a dimension change with a Found compass, teleport (like the needle and the HUD distance)
  uses the same x/z in the new dimension.
- We (U4): sea level is 63 in the overworld and the dimension's sea level elsewhere; the build
  height is the dimension's (min y .. max y, exclusive); blocks outside it count as free; the
  y 256 fallback is clamped to the max y. Free = air, `minecraft:fire` / `minecraft:soul_fire`, a
  liquid material, or a block that can be built over. NC loads the target chunk synchronously; we
  move the player to the target column above the build height, wait (up to 20 s) until the chunk is
  fully loaded, then probe and teleport (on timeout, logout or mod disable the player goes back;
  a player who dies or is moved away meanwhile is left alone). Log line:
  `Player <name> teleported to <x> <y> <z>`.

### 3.6 Biome catalog

- Every registered biome, except biomes hidden from locators (NC `c:hidden_from_locator_selection`)
  and the config blacklist.
- Per biome: id, dimensions whose generator can produce it, tags, source (namespace), temperature,
  downfall ("Rainfall"), XP cost.
- XP cost: the first `perBiomeXpLevels` entry whose glob matches the id (entries `"glob,levels"`,
  levels capped to 3, malformed entries skipped), else `defaultXpLevels`.
- Globs (blacklist and XP): anchored; `*` = any run of characters, `?` = one character
  (`core::globMatch`).

We (U2, `src/feature/biomes/`, `src/core/BiomeText.*`):

- **Ids** are the Bedrock registry names in the `minecraft` namespace (`minecraft:plains`,
  `minecraft:hell` for Nether Wastes, `minecraft:roofed_forest` for Dark Forest, ...), the same
  names the biome source returns, so searches compare them directly. Order: numeric biome id.
- **Hidden** (the Bedrock equivalent of `c:hidden_from_locator_selection`, which is empty in
  vanilla Java, so NC's Java list holds exactly the biomes the generators produce): the
  registry's unnamed placeholder biome, and the vanilla legacy biomes that Bedrock still registers
  for old worlds but no current generator produces (`core::isLegacyBedrockBiome`):
  `bamboo_jungle_hills`, `birch_forest_hills`, `birch_forest_hills_mutated`, `cold_taiga_hills`,
  `cold_taiga_mutated`, `deep_warm_ocean`, `desert_hills`, `desert_mutated`, `extreme_hills_edge`,
  `extreme_hills_plus_trees_mutated`, `forest_hills`, `ice_mountains`, `jungle_edge_mutated`,
  `jungle_hills`, `jungle_mutated`, `legacy_frozen_ocean`, `mega_taiga_hills`, `mesa_plateau`,
  `mesa_plateau_mutated`, `mesa_plateau_stone_mutated`, `mushroom_island_shore`,
  `redwood_taiga_hills_mutated`, `roofed_forest_mutated`, `savanna_plateau_mutated`,
  `swampland_mutated`, `taiga_hills`, `taiga_mutated`. A search for them could never succeed.
  Add-on biomes are never hidden. The remaining vanilla list is Java's 1.21 biome list (Bedrock
  has one End biome, `the_end`) plus Bedrock-only content.
- **Dimensions** come from the Bedrock biome tags: `overworld` or `overworld_generation` ->
  `minecraft:overworld`, `nether` -> `minecraft:nether`, `the_end` -> `minecraft:the_end` (order
  Overworld, Nether, End). Bedrock's generator queries (`BiomeSource::hasBiomeById`,
  `BiomeRegistry::getBiomesInDimension`) do not answer this reliably (measured on 1.26.51: the
  former is false for every biome, the latter lists a handful of legacy-layer biomes). Unlike NC
  on a Java superflat world, a flat world still lists every Overworld biome as Overworld.
- **Tags**: Bedrock biome tags are not Java's `is_*` tags. We map them so that each vanilla biome
  gets exactly its Java 1.21 `minecraft:is_*` tags (minus `is_overworld`), in alphabetical order
  (Java's tag order is unspecified; this makes Deep Ocean read "Deep Ocean", not "Ocean"):

  | NC tag | Bedrock tags |
  | --- | --- |
  | `badlands` | `mesa` |
  | `beach` | `beach` without `stone` |
  | `deep_ocean` | `ocean` + `deep` |
  | `end` | `the_end` |
  | `forest` | `forest`, `flower_forest`, `grove` or `pale_garden`, without `taiga` / `extreme_hills` |
  | `hill` | `extreme_hills` |
  | `jungle` | `jungle` |
  | `mountain` | `mountains` without `grove` |
  | `nether` | `nether` |
  | `ocean` | `ocean` |
  | `river` | `river` |
  | `savanna` | `savanna` |
  | `taiga` | `taiga` |

  Add-on biomes that use the vanilla tags get the same tags.
- Temperature and downfall are the biome's `mTemperature` / `mDownfall`.
- The catalog is rebuilt lazily on first use per world (`game::db::onWorldReady`) and after
  `/naturescompass reload`, logging `Biome catalog: <N> searchable biomes (<H> hidden, <B> blacklisted)`.

### 3.7 Durability

- `compassDurability` 0 (default): no durability, no bar, never broken.
- Otherwise `damage` counts successful searches (not in creative). Broken = damage >=
  compassDurability: the compass can't open the GUI or search; sneak-use still resets it. Repair
  with the repair recipe.
- Bar (NC): only when damage > 0; width clamp(round(13 * (1 - damage / max)), 0, 13), colour HSV
  hue max(0, (1 - damage / max) / 3), green -> red. We show the Bedrock durability bar with the
  same fraction (U4: mirror `damage` onto the item's Bedrock damage, scaled to the item's
  `max_durability`, and add the durability component to the item JSON if needed; the vanilla
  repair paths (anvil, grindstone, crafting-grid combine) must not apply).
- We (U4): the item has `minecraft:durability` with `max_durability` 10000 and `damage_chance`
  0..0 (vanilla use never damages it). Every 10 ticks (and on `actions::refreshDurability`) each
  compass a player carries gets the Bedrock damage round(10000 * damage / compassDurability)
  (at least 1 while damaged; 9999 when broken: an empty bar that a stray vanilla durability hit
  can't turn into a destroyed item; 0 without damage or with durability 0).
  `damage` in the compass data stays the only truth: a vanilla repair (anvil / grindstone)
  only changes the bar, which the next sync puts back; broken-ness never reads the Bedrock damage.

### 3.8 Needle

- NC model: 32 frames (`_00` .. `_31`), dispatched on the compass angle (vanilla compass logic,
  wobble 0.8). Angle mapping: 0 -> frame 16, 0.5 -> 17, ..., 14.5 -> 31, 15.5 -> 00, ...,
  30.5 -> 15, 31.5 -> 16.
- Target: state Found -> (found_x, 0, found_z) in the holder's current dimension; any other state
  -> the world spawn (like a vanilla compass). No special searching animation.
- Random spin when the target is in another dimension, the item has no living holder (item frame,
  dropped) or the holder is at ~0 distance from the target.
- Pointing: atan2(dz, dx) / 2π; for the local player the wobbler eases toward 0.5 - (yaw / 360 -
  0.25); for other holders 0.5 - (yaw / 360 - 0.25 - angle), no wobble.
- We: the client build animates the needle in `src/client/needle/`: our item's
  `getAnimationFrameFor(holder, asItemEntity, stack, ...)` returns the needle frame (angle math in
  `core/CompassAngle.h`; installed as a private vtable copy on our Item object, because the game's
  function body is identical-code-folded with unrelated functions and can't be detoured) and a
  detour of `ComponentItem::$getIconInfo(stack, frame, ...)` returns entry `1 + frame` of the
  texture key `naturescompass`. The key is a texture array
  (`item_texture.d/needle.json`): entry 0 is the plain icon (needle up, what vanilla clients see),
  entries 1..32 are the frames `naturescompass_00` .. `_31` (original art, `tools/textures/compass.ps1`).
- The needle state (local-player wobble, spin wobble) is shared by every Nature's Compass, like
  NC's single property function; the wobble advances once per client game tick.
- Holders: the local player (wobble), other players and mobs (no wobble) and, with no living
  holder (dropped item, item frame: `asItemEntity` or no holder), the random spin with seed 0.
  The seed of a holder is its runtime id. The spawn target and its dimension check are the vanilla
  compass's own (`CompassSpriteCalculator::pointTowardsSpawnPoint` / `isInOverworldDimension`).
- The client reads `compass_state`, `found_x` and `found_z` from the stack's user data as synced to
  it, in local worlds and on a BDS.
- Debug command (op): `/naturescompass needle found <x> <z>` marks the held compass Found at x/z;
  client build: `/naturescompass needle frame` logs `needle: frame <n> (yaw <y>)` for the local
  player's held compass.

### 3.9 Edge cases

- Creative: no XP cost, no durability damage; does not grant teleport.
- Dropping a compass mid-search: the dropped copy stays Searching until it is sneak-used (the
  worker can't find it in the player's inventory and stops silently).
- Only the held compass matters for GUI, actions, HUD and needle.
- Copies of one compass (creative pick-block / duplication) share its `uid`: a search started on
  one copy stops a search running on another copy of the same player, and a worker writes to the
  first copy `findCompass` returns (held ones first). Accepted (NC's single worker is worse).

## 4. Screens

### 4.1 Display helpers

- **Biome name**: the biome's translated name (`biome.<ns>.<path>`, U2's lang file). With
  `fixBiomeNames` a space is inserted before every uppercase letter that follows a lowercase one
  ("ForestHills" -> "Forest Hills"). The fix applies to the GUI list and its filter only; sorting
  and the HUD use the raw name.
- **Dimensions**: names of the dimensions that can produce the biome: the translation of
  `dimension.<ns>.<path>`, else the path with `_` -> space, capitalised ("Overworld",
  "The Nether", "The End"); joined with ", "; empty when none.
- **Tags**: the biome's `is_*` tags except `is_overworld`, `is_` stripped, cut at the first `/`.
  For each: if `biome.<ns>.<x>` has a translation, the result is ONLY that string (returned at
  once); else if `category.<ns>.<x>` has one, only that; else the path with `_` -> space,
  capitalised, goes into a sorted set. The set joined with ", ", or "None" when empty.
- **Source**: "Minecraft" for the `minecraft` namespace, else the add-on / mod name or the raw
  namespace.

We: biome names are `biome.minecraft.<bedrock name>` in `assets/lang/<locale>/biomes.json`:
Java's current English and official Chinese names for every Bedrock biome with a Java equivalent
(`hell` "Nether Wastes" / "下界荒地", `roofed_forest` "Dark Forest" / "黑森林", `mesa_plateau_stone`
"Wooded Badlands" / "疏林恶地", ...), the pre-1.18 Java names for legacy biomes ("Desert Hills",
"Gravelly Mountains+", ...) and names in the same style for Bedrock-only biomes. An id without a
translation shows its path capitalised ("my_addon:crystal_wastes" -> "Crystal Wastes").
`biome.minecraft.badlands` ("Badlands") also exists because it is an NC tag name. Dimension names
are `dimension.minecraft.{overworld,nether,the_end}` (Java has no such keys, so NC always shows the
English fallback; we translate them). Rainfall / Temperature values use `core::javaFloatToString` (`rainfallText` / `temperatureText` in `Biomes.h`)
(Java `Float.toString`: `0.8`, `2.0`, `-0.5`, `1.0E-4`). Add-on sources show the raw namespace.

### 4.2 Select Biome

NC's only screen (there is no info screen in 3.3.1). NC layout (GUI pixels):

| Element | x | y | w | h | Text |
| --- | --- | --- | --- | --- | --- |
| Title | centred on x = 65 | 15 | | | "Select Biome", white |
| Search | 10 | 40 | 110 | 20 | "Search"; active only when an entry is selected |
| Search for Next | 10 | 65 | 110 | 20 | visible if maxNextSearches > 0; active only when the selected entry is the compass's Found biome |
| Sort By | 10 | 100 (65 if Next is hidden) | 110 | 20 | "Sort By: <mode>" |
| Teleport | width - 120 | 10 | 110 | 20 | visible if canTeleport; active by the same rule as Next |
| Cancel | 10 | height - 30 | 110 | 20 | "Cancel"; closes |
| Search box | 130 | 10 | 140 | 20 | hint "Search" in 0xff808080 |
| Biome list | 130 | 40 | width - 140 | height - 50 | rows 50 px |

- Buttons have no texture: filled rectangles 0x7F000000 normal, 0xB8000000 hovered, 0x2B000000
  inactive; white centred text. List rows use the same fills (hovered = selected, inactive =
  can't afford). Scrollbar 6 px.
- Row lines at y + 3, + 14, + 25, + 36, x + 5:
  1. display name (fixBiomeNames): white, 0xff808080 if unaffordable;
  2. sort Name / Source / Tags / Dimension: `Dimension: <dims>`; otherwise `<Mode>: <value>`,
     e.g. `Levels: 2`, `Rainfall: 0.8`, `Temperature: 0.8` (Java `Float.toString`: shortest
     representation with at least one decimal);
  3. `Tags: <tags>`, cut with "..." to fit the row;
  4. `Source: <source>`.
  Lines 2-4 in 0xff808080 (0xff555555 unaffordable). XP cost > 0: the enchanting-table level
  icon (`level_N` / `level_N_disabled`) at the right edge, size 20, margin 15.
- Affordable = creative or XP level >= cost. Clicking an affordable row selects it;
  double-clicking searches. Unaffordable rows can't be selected.
- Search / Search for Next: click sound, action, screen closes. Teleport: action, screen closes.
- Sort modes cycle Name, Source, Tags, Dimension, Levels (cost as string), Rainfall, Temperature,
  back to Name. The list is always sorted by name first, then stable-sorted by the mode,
  ascending.
- Filter (search box), recomputed on every change, case-insensitive "contains": `$term` matches
  the tags text, `@term` matches the source, anything else the display name.
- On open the found biome (state Found) is pre-selected (we: only when affordable); the filter is empty and the list
  scrolled to the top.

**Our form** (deviation 1.3.1): title "Select Biome"; a text field (filter, same `$` / `@`
rules); a sort selector cycling the same modes ("Sort By: <mode>"); the biome rows as buttons
showing the four lines (unaffordable rows greyed and disabled, level cost shown); the selected
row highlighted; "Search", "Search for Next" (when visible) and "Teleport" (when visible) with
NC's active rules; "Cancel" (`string.naturescompass.cancel`). The data-driven `ll::ui` form
updates in place (filter, sort, selection); the classic-form fallback rebuilds the form instead.
U5 owns the details; it reads only the contracts (`Biomes.h`, `Actions.h`, `CompassData.h`).

Implementation (U5, `src/feature/gui/`, model in `src/core/SelectBiome.h`):

- **Model.** One `SelectBiomeModel` per open screen: rows (catalog order), filter, sort mode,
  selection. Changing the filter or the sort rebuilds the list like NC's `refreshList`: the
  selection is dropped and the list starts at the top. Rows the player can't afford stay listed
  (greyed) but can't be selected. A click on the selected row is NC's double click (Search). The
  data the screen shows (catalog, XP level, compass state, `canTeleport`) is read once when it
  opens, like NC's sync packet.
- **Data-driven form** (default, `ll::ui::CustomForm`, rendered by the client's Ore UI): a text
  field "Search" (the client reports every edit, so the list re-filters live as in NC), then
  Search, Search for Next, Sort By, Teleport, Cancel, a divider and one button + one label per
  biome. Ore UI shows neither formatting codes nor line breaks in button labels, so a row is a
  button with line 1 (`<name>`, `[<levels>]` when the cost is > 0, `> ... <` around the selected
  row) and a label below it with lines 2-4 (grey). Unaffordable rows are disabled buttons
  (greyed by the client). Hidden/disabled states are observables, so nothing is re-sent. The
  JSON-UI skin cannot reach Ore UI screens.
- **Classic form** (fallback when the data-driven form can't be shown; `/naturescompass gui
  classic|ddui|auto` switches it for testing): an `ll::form::SimpleForm` re-sent after every
  click. Its title and buttons carry invisible formatting-code markers
  (`src/feature/gui/GuiMarkers.h`); the RP skin (`ui/nl_gui_screen.json` + the `ui/server_form.json`
  modification, scoped by the title marker) draws NC's layout from them: title at the top left,
  Search / Search for Next / Sort By (10 px lower under Next) in the left column, Teleport at the
  top right, Cancel at the bottom left, the search box at (130, 10) and the scrolling list of
  4-line rows (with the enchanting table's level icons `textures/ui/dust_(un)selectable_N`) on
  the right, with NC's fills (0x7F / 0xB8 hovered or selected / 0x2B inactive or unaffordable).
  The search box opens a one-field input form; the filter applies on submit. Without the RP the
  classic form is a plain vanilla button list (rows show their first lines).
- Log lines (e2e): `Select Biome opened for <player> (<n> biomes, <data-driven|classic> form,
  selected '<id>')`, `Select Biome: <player> filter '<text>' (<n> listed)`, `... sort by <Mode>`,
  `... selected <id>`, `... chose <Search|Search for Next|Teleport> (<id>)`, `... chose Cancel`,
  `... closed the screen`.

### 4.3 HUD

- Drawn above the boss bar. Uses the compass in the main hand, then the off hand. Hidden when the
  HUD is hidden (F1), with the debug screen, and when any screen is open, except the chat screen
  when `displayWithChatOpen`.
- Line n at y = 5 + (n + overlayLineOffset) * 9 + 2 (GUI pixels). LEFT: x = 7; RIGHT:
  right-aligned at screen width - 7. Text with shadow; labels white 0xFFFFFF, values grey
  0xAAAAAA.

| State | Lines (n: text) |
| --- | --- |
| Searching | 0 "Status", 1 "Searching", 3 "Biome", 4 name, 6 "Radius", 7 search_radius |
| Found | 0 "Status", 1 "Found", 3 "Biome", 4 name; if display_coords: 6 "Coordinates", 7 "<x>, <z>", 9 "Distance", 10 live integer horizontal distance from the player's block position |
| Not Found | 0 "Status", 1 "Not Found", 3 "Biome", 4 name, 6 "Radius", 7 search_radius, 9 "Samples", 10 samples |
| Inactive | nothing |

- The biome name on the HUD is the raw translated name (no fixBiomeNames).
- We (deviation 1.3.2): the client build draws exactly this for its local player. Other players
  get the block as a marked title/actionbar string from the server, rendered by the RP's
  `hud_screen.json` modification at the same place, with the same colours and line spacing.

## 5. Configuration

JSON files under `config/` in the mod folder, created with defaults (`NL_REGISTER_CONFIG`,
`src/feature/compass/CompassConfig.h`). Out-of-range values are clamped when read. Bedrock /
LeviLamina config files carry no comments; the comments below are NC's and are repeated in the
README.

### 5.1 `naturescompass.json` (NC "General")

| Key | Type | Default | Range | NC comment |
| --- | --- | --- | --- | --- |
| `allowTeleport` | bool | `true` | | "Allows a player to teleport to a located biome when in creative mode, opped, or in cheat mode." (We: opped or cheat mode; see 1.3.7.) |
| `maxNextSearches` | int | `25` | 0..10000 | "The maximum number of times a player can search for the next instance of a located biome. Set to 0 to disable searching for additional biome instances and make the compass always locate the nearest biome." |
| `displayCoordinates` | bool | `true` | | "Allows players to view the precise coordinates and distance of a located structure on the HUD, rather than relying on the direction the compass is pointing." |
| `radiusModifier` | int | `2500` | 0..1000000 | "biomeSize * radiusModifier = maxSearchRadius. Raising this value will increase search accuracy but will potentially make the process more resource intensive." |
| `sampleSpaceModifier` | int | `16` | 0..1000000 | "biomeSize * sampleSpaceModifier = sampleSpace. Lowering this value will increase search accuracy but will make the process more resource intensive." |
| `biomeBlacklist` | string list | `[]` | | "A list of biomes that the compass will not be able to search for, specified by resource location. The wildcard character * can be used to match any number of characters, and ? can be used to match one character. Ex: ["minecraft:savanna", "minecraft:desert", "minecraft:*ocean*"]" |
| `defaultXpLevels` | int | `0` | 0..3 | "The default number of XP levels consumed when searching for a biome. Individual biomes can be configured via perBiomeXpLevels." |
| `perBiomeXpLevels` | string list | `[]` | | "A list of per-biome XP level costs that override defaultXpLevels, specified as comma-separated "biome_id,num_levels" pairs. Biomes not listed here use defaultXpLevels. Max of 3 levels. Ex: ["minecraft:deep_dark,3", "minecraft:end*,2", "minecraft:*caves,3"]" |
| `maxSamples` | int | `50000` | 0..1000000 | "The maximum number of samples to be taken when searching for a biome." |
| `compassDurability` | int | `0` | 0..10000 | "The number of successful biome searches before the compass breaks and must be repaired. Set to 0 to disable durability." |

### 5.2 `naturescompass_client.json` (NC "Client")

| Key | Type | Default | Range | NC comment |
| --- | --- | --- | --- | --- |
| `displayWithChatOpen` | bool | `true` | | "Displays Nature's Compass information even while chat is open." |
| `fixBiomeNames` | bool | `true` | | "Fixes biome names by adding missing spaces. Ex: ForestHills becomes Forest Hills" |
| `overlayLineOffset` | int | `1` | 0..50 | "The line offset for information rendered on the HUD." |
| `overlaySide` | string | `"LEFT"` | `LEFT`, `RIGHT` (case-insensitive; anything else = LEFT) | "The side for information rendered on the HUD. Ex: LEFT, RIGHT" |

In local worlds the client build reads this section (the integrated server and the client share
the mod folder). On BDS the server's copy drives the fallback HUD and the form's name fixing;
a modded client on BDS uses its own copy for its native HUD.

`/naturescompass reload` reloads both sections (and rebuilds the biome catalog).

## 6. Commands

NC has no commands. Ours (`/naturescompass`, alias `/natureslodestone`) are tooling only:

| Command | Who | What |
| --- | --- | --- |
| `/naturescompass reload` | operators, console | reload configs and translations |
| `/naturescompass selftest ["glob"]` | operators, console | run the in-game selftests |
| `/naturescompass damage <n>` | operators | debug (U4): set the held compass's `damage` (durability bar in e2e) |

Units may add debug sub-commands through `NL_REGISTER_SUBCOMMAND`; there is no gameplay command.
NC has no advancements besides the two recipe unlocks (2.2), no sounds of its own (Search and
Search for Next play the UI click), no loot.

## 7. Strings

Mod strings: `assets/lang/<locale>/compass.json`, used through `nl::tr` / `nl::trFor` /
`nl::client::ctr`. Every key is NC's `string.naturescompass.<key>` and NC's text, verbatim; keys
the 3.3.1 code no longer uses are kept for completeness. `cancel` is ours (NC uses vanilla
`gui.cancel`). Item name: RP `texts.d/<locale>/compass.lang`, key
`item.natureslodestone:naturescompass.name`. Biome and dimension names: U2's `biomes.json`.

| Key (`string.naturescompass.`) | en_US | zh_CN | Used by |
| --- | --- | --- | --- |
| `selectBiome` | Select Biome | 选择生物群系 | GUI title |
| `search` | Search | 搜索 | GUI button, search-box hint |
| `searchForNext` | Search for Next | 搜索下一个 | GUI button |
| `sortBy` | Sort By | 排序依据 | GUI button ("Sort By: <mode>") |
| `teleport` | Teleport | 传送 | GUI button |
| `cancel` | Cancel | 取消 | GUI button (ours) |
| `name` | Name | 名称 | sort mode |
| `source` | Source | 来源 | sort mode, row line 4 |
| `tags` | Tags | 标签 | sort mode, row line 3 |
| `dimension` | Dimension | 维度 | sort mode, row line 2 |
| `levels` | Levels | 等级 | sort mode |
| `rainfall` | Rainfall | 降水量 | sort mode |
| `temperature` | Temperature | 温度 | sort mode |
| `none` | None | 无 | empty tags |
| `status` | Status | 状态 | HUD |
| `searching` | Searching | 搜索中 | HUD |
| `found` | Found | 已发现 | HUD |
| `notFound` | Not Found | 未发现 | HUD |
| `biome` | Biome | 生物群系 | HUD |
| `radius` | Radius | 半径 | HUD |
| `coordinates` | Coordinates | 坐标 | HUD |
| `distance` | Distance | 距离 | HUD |
| `samples` | Samples | 样本 | HUD |
| `back` | Back | 返回 | unused |
| `info` | Info | 信息 | unused |
| `cold` / `medium` / `warm` | Cold / Medium / Warm | 寒冷 / 适中 / 温暖 | unused |
| `veryLow` / `low` / `average` / `high` / `veryHigh` | Very Low / Low / Average / High / Very High | 非常低 / 低 / 适中 / 高 / 非常高 | unused |
| `category` | Category | 类型 | unused |
| `baseHeight` | Base Height | 高度 | unused |
| `climate` | Climate | 气候 | unused |
| `fillerBlock` | Filler Block | 地基组成 | unused |
| `heightVariation` | Height Variation | 高度变化 | unused |
| `highHumidity` | High Humidity | 海拔湿度 | unused |
| `precipitation` | Precipitation | 降水类型 | unused |
| `topBlock` | Top Block | 地表组成 | unused |
| `snow` / `rain` | Snow / Rain | 雪 / 雨 | unused |

| RP texts key | en_US | zh_CN |
| --- | --- | --- |
| `item.natureslodestone:naturescompass.name` | Nature's Compass | 自然罗盘 |

Composite texts are built in code as NC does: `"<label>: <value>"` (e.g. `Sort By: Name`,
`Dimension: Overworld`, `Levels: 2`) and `"<x>, <z>"`.

## 8. Not ported

- **Animated needle for vanilla clients.** Bedrock item icons can't be animated per stack from a
  resource pack; vanilla clients (on BDS without the client build) see the static icon. The
  client build animates it.
- **Exact GUI widgets.** A server form can't reproduce NC's pixel layout (free-positioned
  buttons, double-click, hover fills, 6 px scrollbar, level icons in rows) for vanilla clients;
  the RP skin approximates it (4.2).
- **Mixed saplings in one recipe** (1.3.8).
- **Recipe group** "tools" (no Bedrock equivalent).
- **Client-trusted search start position.** We use the server's view of the player position.
- **Item frames / dropped compasses spinning** for vanilla clients (part of the needle).
- **Biome sources of other mods** ("Source: <mod name>"): Bedrock has no Java mods; add-on biome
  namespaces show as the raw namespace.

## 9. Work plan

Feature units, each with its own directory, lang fragment and selftest range (U<n> owns selftest
dx 100n .. 100n+99). Contracts live in `src/feature/compass/`; each unit replaces the default
implementation in its own directory without touching other units' files.

| Unit | Name | Owns | Depends on | Selftest dx |
| --- | --- | --- | --- | --- |
| U1 | `compass` | this spec; `src/game/Items.*`; `src/feature/compass/` (item data, config, contract headers); BP item + recipes; `assets/lang/*/compass.json`; RP `texts.d/*/compass.lang`; `ui_defs.d` merging in `tools/xmake/assemble.lua`; default stubs in the directories below | — | 100..199 |
| U2 | `biomes` | `src/feature/biomes/` (catalog: tags, hidden filter, names, sources, dimensions), `src/core/BiomeText.h/.cpp` + tests (name fixing, capitalising, tag text), `assets/lang/*/biomes.json` (biome + dimension names) | U1 | 200..299 |
| U3 | `search` | `src/feature/search/` (worker manager, tick budget, spiral sampling, Search for Next connectivity), `src/core/Spiral.h` + tests | U1 (U2 for biome lookup by id) | 300..399 |
| U4 | `actions` | `src/feature/actions/` (use handling incl. on blocks, sneak reset, broken; search / searchForNext / teleport incl. XP cost, durability damage + bar, permission, safe-y teleport) | U1, U3 | 400..499 |
| U5 | `gui` | `src/feature/gui/` (Select Biome via `ll::ui` data-driven form, classic `ll::form` fallback), RP `ui/nl_gui*.json` + `ui_defs.d/gui.json` | U1, U2, U4 | 500..599 |
| U6 | `hud` | `src/client/hud/` (native overlay), `src/feature/hud/` (server fallback: marked title/actionbar + RP `ui/hud_screen.json` modification), `ui_defs.d/hud.json` | U1, U2 | 600..699 |
| U7 | `needle` | `tools/textures/compass.ps1` (32 needle frames + icon), `item_texture.d/needle.json` (texture key `naturescompass`), `src/client/needle/` (`ComponentItem::$getIconInfo` hook) | U1 | 700..799 |
| U8 | `showcase` | full-flow e2e (server + client, local + remote), `docs/showcase/`, README, CHANGELOG | all | 800..899 |

Waves: U1 first (wave 0); U2-U7 in parallel (wave 1) on top of U1's contracts and stubs; U8 last
(wave 2).

### 9.1 Contracts (src/feature/compass/)

| Header | Namespace | Default implementation | Owner |
| --- | --- | --- | --- |
| `CompassData.h` | `nl::feature::compass` | `CompassData.cpp` (final) | U1 |
| `CompassConfig.h` | `nl::feature::compass` | `CompassConfig.cpp` (final) | U1 |
| `Biomes.h` | `nl::feature::biomes` | `src/feature/biomes/Biomes.cpp` (registry enumeration, no tags) | U2 |
| `Search.h` | `nl::feature::search` | `src/feature/search/Search.cpp` (writes Searching, Not Found one tick later) | U3 |
| `Actions.h` | `nl::feature::actions` | `src/feature/actions/Actions.cpp` (item use -> GUI / sneak reset, XP, stub teleport) | U4 |
| `Gui.h` | `nl::feature::gui` | `src/feature/gui/Gui.cpp` (classic button form) | U5 |
| `Hud.h` | `nl::feature::hud` | `src/feature/hud/Hud.cpp` (no-op) | U6 |

A unit may add functions to a contract header it depends on only compatibly (add, never change
or remove) and calls that out in its PR.

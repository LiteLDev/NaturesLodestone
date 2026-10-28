#pragma once

// Unit-internal helpers of the biome catalog (not part of the contract).

namespace nl::feature::biomes {

/// Forces a rebuild of the catalog on its next access (selftests that change the config).
void invalidateCatalog();

} // namespace nl::feature::biomes

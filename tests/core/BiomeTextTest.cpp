#include <gtest/gtest.h>

#include <limits>
#include <map>

#include "core/BiomeText.h"

using namespace nl::core;

TEST(BiomeText, Ids) {
    EXPECT_EQ(idNamespace("minecraft:plains"), "minecraft");
    EXPECT_EQ(idNamespace("plains"), "minecraft");
    EXPECT_EQ(idNamespace("foo:bar"), "foo");
    EXPECT_EQ(idPath("minecraft:plains"), "plains");
    EXPECT_EQ(idPath("plains"), "plains");
    EXPECT_EQ(fullBiomeId("plains"), "minecraft:plains");
    EXPECT_EQ(fullBiomeId("foo:bar"), "foo:bar");
}

TEST(BiomeText, FixBiomeName) {
    EXPECT_EQ(fixBiomeName("ForestHills"), "Forest Hills");
    EXPECT_EQ(fixBiomeName("Forest Hills"), "Forest Hills");
    EXPECT_EQ(fixBiomeName("TheEND"), "The END");
    EXPECT_EQ(fixBiomeName("ABC"), "ABC");
    EXPECT_EQ(fixBiomeName("aB1C"), "a B1C");
    EXPECT_EQ(fixBiomeName(""), "");
    EXPECT_EQ(fixBiomeName("下界荒地"), "下界荒地");
}

TEST(BiomeText, CapitalizePath) {
    EXPECT_EQ(capitalizePath("snowy_plains"), "Snowy Plains");
    EXPECT_EQ(capitalizePath("the_nether"), "The Nether");
    EXPECT_EQ(capitalizePath("deep_ocean"), "Deep Ocean");
    EXPECT_EQ(capitalizePath("x"), "X");
    EXPECT_EQ(capitalizePath("crystal_wastes_2"), "Crystal Wastes 2");
}

TEST(BiomeText, JavaFloatToString) {
    EXPECT_EQ(javaFloatToString(0.8f), "0.8");
    EXPECT_EQ(javaFloatToString(0.95f), "0.95");
    EXPECT_EQ(javaFloatToString(2.0f), "2.0");
    EXPECT_EQ(javaFloatToString(-0.5f), "-0.5");
    EXPECT_EQ(javaFloatToString(0.0f), "0.0");
    EXPECT_EQ(javaFloatToString(-0.0f), "-0.0");
    EXPECT_EQ(javaFloatToString(0.05f), "0.05");
    EXPECT_EQ(javaFloatToString(100.0f), "100.0");
    EXPECT_EQ(javaFloatToString(123.25f), "123.25");
    EXPECT_EQ(javaFloatToString(0.001f), "0.001");
    EXPECT_EQ(javaFloatToString(0.0001f), "1.0E-4");
    EXPECT_EQ(javaFloatToString(1.0e7f), "1.0E7");
    EXPECT_EQ(javaFloatToString(9999999.0f), "9999999.0");
    EXPECT_EQ(javaFloatToString(1.5e10f), "1.5E10");
    EXPECT_EQ(javaFloatToString(0.1f + 0.2f), "0.3");
    EXPECT_EQ(javaFloatToString(1.0f / 3.0f), "0.33333334");
    EXPECT_EQ(javaFloatToString(std::numeric_limits<float>::infinity()), "Infinity");
    EXPECT_EQ(javaFloatToString(-std::numeric_limits<float>::infinity()), "-Infinity");
    EXPECT_EQ(javaFloatToString(std::numeric_limits<float>::quiet_NaN()), "NaN");
}

TEST(BiomeText, NcTagsFromBedrock) {
    using V = std::vector<std::string>;
    EXPECT_EQ(ncTagsFromBedrock({"animal", "monster", "overworld", "plains"}), V{});
    EXPECT_EQ(ncTagsFromBedrock({"animal", "forest", "monster", "overworld", "oak_forest"}), V{"forest"});
    EXPECT_EQ(ncTagsFromBedrock({"animal", "forest", "monster", "overworld", "taiga"}), V{"taiga"});
    EXPECT_EQ(ncTagsFromBedrock({"deep", "monster", "ocean", "overworld"}), (V{"deep_ocean", "ocean"}));
    EXPECT_EQ(ncTagsFromBedrock({"beach", "monster", "overworld", "stone"}), V{});
    EXPECT_EQ(ncTagsFromBedrock({"beach", "cold", "monster", "overworld"}), V{"beach"});
    EXPECT_EQ(ncTagsFromBedrock({"mountains", "cold", "overworld", "grove"}), V{"forest"});
    EXPECT_EQ(ncTagsFromBedrock({"mountains", "overworld", "meadow"}), V{"mountain"});
    EXPECT_EQ(ncTagsFromBedrock({"animal", "extreme_hills", "forest", "mountain", "overworld"}), V{"hill"});
    EXPECT_EQ(ncTagsFromBedrock({"mesa", "overworld", "plateau", "stone"}), V{"badlands"});
    EXPECT_EQ(ncTagsFromBedrock({"bamboo", "jungle", "overworld"}), V{"jungle"});
    EXPECT_EQ(ncTagsFromBedrock({"overworld", "savanna", "plateau"}), V{"savanna"});
    EXPECT_EQ(ncTagsFromBedrock({"frozen", "overworld", "river"}), V{"river"});
    EXPECT_EQ(ncTagsFromBedrock({"nether", "nether_wastes"}), V{"nether"});
    EXPECT_EQ(ncTagsFromBedrock({"the_end"}), V{"end"});
    EXPECT_EQ(ncTagsFromBedrock({"monster", "overworld", "pale_garden"}), V{"forest"});
    EXPECT_EQ(ncTagsFromBedrock({"flower_forest", "mutated", "overworld"}), V{"forest"});
}

TEST(BiomeText, Dimensions) {
    using V = std::vector<std::string>;
    EXPECT_EQ(dimensionsFromBedrockTags({"overworld", "plains"}), V{"minecraft:overworld"});
    EXPECT_EQ(dimensionsFromBedrockTags({"desert", "overworld_generation"}), V{"minecraft:overworld"});
    EXPECT_EQ(dimensionsFromBedrockTags({"nether", "basalt_deltas"}), V{"minecraft:nether"});
    EXPECT_EQ(dimensionsFromBedrockTags({"the_end"}), V{"minecraft:the_end"});
    EXPECT_EQ(dimensionsFromBedrockTags({}), V{});
    EXPECT_EQ(dimensionFallbackName("minecraft:overworld"), "Overworld");
    EXPECT_EQ(dimensionFallbackName("minecraft:nether"), "The Nether");
    EXPECT_EQ(dimensionFallbackName("minecraft:the_nether"), "The Nether");
    EXPECT_EQ(dimensionFallbackName("minecraft:the_end"), "The End");
    EXPECT_EQ(dimensionFallbackName("foo:deep_space"), "Deep Space");
}

TEST(BiomeText, LegacyBiomes) {
    EXPECT_TRUE(isLegacyBedrockBiome("desert_hills"));
    EXPECT_TRUE(isLegacyBedrockBiome("legacy_frozen_ocean"));
    EXPECT_TRUE(isLegacyBedrockBiome("mesa_plateau"));
    EXPECT_FALSE(isLegacyBedrockBiome("mesa_plateau_stone"));
    EXPECT_FALSE(isLegacyBedrockBiome("plains"));
    EXPECT_FALSE(isLegacyBedrockBiome("birch_forest_mutated"));
    EXPECT_FALSE(isLegacyBedrockBiome("redwood_taiga_mutated"));
}

TEST(BiomeText, TagsLine) {
    std::map<std::string, std::string> tr{
        {"biome.minecraft.forest",     "Forest"    },
        {"biome.minecraft.deep_ocean", "Deep Ocean"},
        {"biome.minecraft.ocean",      "Ocean"     },
        {"category.minecraft.nether",  "Nether!"   },
    };
    TranslationLookup lookup = [&](std::string const& key) -> std::optional<std::string> {
        auto it = tr.find(key);
        if (it == tr.end()) return std::nullopt;
        return it->second;
    };
    EXPECT_EQ(tagsLine({}, "minecraft", lookup, "None"), "None");
    EXPECT_EQ(tagsLine({"forest"}, "minecraft", lookup, "None"), "Forest");
    // The first translated tag wins at once, even after untranslated ones.
    EXPECT_EQ(tagsLine({"mountain", "forest"}, "minecraft", lookup, "None"), "Forest");
    EXPECT_EQ(tagsLine({"deep_ocean", "ocean"}, "minecraft", lookup, "None"), "Deep Ocean");
    EXPECT_EQ(tagsLine({"nether"}, "minecraft", lookup, "None"), "Nether!");
    // Untranslated tags: capitalised, sorted, unique.
    EXPECT_EQ(tagsLine({"mountain", "hill", "hill"}, "minecraft", lookup, "None"), "Hill, Mountain");
    EXPECT_EQ(tagsLine({"forest"}, "foo", lookup, "None"), "Forest");
}

TEST(BiomeText, Source) {
    EXPECT_EQ(sourceName("minecraft"), "Minecraft");
    EXPECT_EQ(sourceName("my_addon"), "my_addon");
}

TEST(BiomeText, XpEntries) {
    using P = std::pair<std::string, int>;
    EXPECT_EQ(parseXpEntry("minecraft:deep_dark,3"), P("minecraft:deep_dark", 3));
    EXPECT_EQ(parseXpEntry(" minecraft:end* , 2 "), P("minecraft:end*", 2));
    EXPECT_EQ(parseXpEntry("minecraft:*caves,7"), P("minecraft:*caves", 3));
    EXPECT_EQ(parseXpEntry("minecraft:plains,-1"), P("minecraft:plains", 0));
    EXPECT_EQ(parseXpEntry("minecraft:plains,+2"), P("minecraft:plains", 2));
    EXPECT_FALSE(parseXpEntry("minecraft:plains"));
    EXPECT_FALSE(parseXpEntry("minecraft:plains,"));
    EXPECT_FALSE(parseXpEntry(",2"));
    EXPECT_FALSE(parseXpEntry("minecraft:plains,two"));
    EXPECT_FALSE(parseXpEntry("minecraft:plains,2.5"));
    EXPECT_FALSE(parseXpEntry("minecraft:plains,2,3"));
    EXPECT_FALSE(parseXpEntry("minecraft:plains,+-2"));
    EXPECT_FALSE(parseXpEntry("minecraft:plains,99999999999"));
}

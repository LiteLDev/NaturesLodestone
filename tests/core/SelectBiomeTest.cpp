#include <gtest/gtest.h>

#include "core/SelectBiome.h"

using namespace nl::core;

namespace {

BiomeRow row(std::string id, std::string name, std::string tags = "None", std::string source = "Minecraft") {
    BiomeRow r;
    r.id          = std::move(id);
    r.rawName     = name;
    r.displayName = std::move(name);
    r.tags        = std::move(tags);
    r.source      = std::move(source);
    r.dimensions  = "Overworld";
    return r;
}

std::vector<std::string> listedIds(SelectBiomeModel const& m) {
    std::vector<std::string> out;
    for (auto i : m.listed()) out.push_back(m.rows()[i].id);
    return out;
}

std::vector<BiomeRow> sample() {
    auto plains        = row("minecraft:plains", "Plains", "None");
    plains.temperature = 0.8f;
    plains.rainfall    = 0.4f;
    plains.xpLevels    = 2;
    auto forest        = row("minecraft:forest", "Forest", "Forest");
    forest.temperature = 0.7f;
    forest.rainfall    = 0.8f;
    forest.xpLevels    = 10; // "10" < "2" as strings
    auto desert        = row("minecraft:desert", "Desert", "Desert");
    desert.temperature = 2.0f;
    desert.rainfall    = 0.0f;
    desert.dimensions  = "";
    auto hills         = row("addon:forest_hills", "ForestHills", "Forest", "addon");
    hills.displayName  = "Forest Hills";
    hills.temperature  = 0.7f;
    hills.rainfall     = 0.8f;
    hills.affordable   = false;
    hills.xpLevels     = 3;
    return {plains, forest, desert, hills};
}

} // namespace

TEST(SelectBiome, SortModesCycle) {
    auto m = BiomeSortMode::Name;
    std::vector<std::string_view> names;
    for (int i = 0; i < kBiomeSortModeCount + 1; ++i) {
        names.push_back(sortModeEnglish(m));
        m = nextSortMode(m);
    }
    EXPECT_EQ(
        names,
        (std::vector<std::string_view>{"Name", "Source", "Tags", "Dimension", "Levels", "Rainfall", "Temperature", "Name"})
    );
    EXPECT_EQ(sortModeKey(BiomeSortMode::Rainfall), "string.naturescompass.rainfall");
    EXPECT_TRUE(showsDimensionLine(BiomeSortMode::Dimension));
    EXPECT_TRUE(showsDimensionLine(BiomeSortMode::Tags));
    EXPECT_FALSE(showsDimensionLine(BiomeSortMode::Levels));
    EXPECT_FALSE(showsDimensionLine(BiomeSortMode::Temperature));
}

TEST(SelectBiome, JavaFloatToString) {
    EXPECT_EQ(javaFloatToString(0.8f), "0.8");
    EXPECT_EQ(javaFloatToString(2.0f), "2.0");
    EXPECT_EQ(javaFloatToString(-0.5f), "-0.5");
    EXPECT_EQ(javaFloatToString(0.0f), "0.0");
    EXPECT_EQ(javaFloatToString(0.95f), "0.95");
    EXPECT_EQ(javaFloatToString(0.05f), "0.05");
    EXPECT_EQ(javaFloatToString(1.5f), "1.5");
    EXPECT_EQ(javaFloatToString(100.f), "100.0");
    EXPECT_EQ(javaFloatToString(0.001f), "0.001");
    EXPECT_EQ(javaFloatToString(1e-5f), "1.0E-5");
    EXPECT_EQ(javaFloatToString(1.25e7f), "1.25E7");
    EXPECT_EQ(javaFloatToString(1234567.f), "1234567.0");
}

TEST(SelectBiome, Ellipsize) {
    EXPECT_EQ(ellipsize("Forest", 10), "Forest");
    EXPECT_EQ(ellipsize("Forest, Mountain", 10), "Forest,...");
    EXPECT_EQ(ellipsize("abcdefghij", 10), "abcdefghij");
    EXPECT_EQ(ellipsize("abcdefghijk", 10), "abcdefg...");
    // Code points, not bytes.
    EXPECT_EQ(ellipsize("森林森林森林", 6), "森林森林森林");
    EXPECT_EQ(ellipsize("森林森林森林森", 6), "森林森...");
    EXPECT_EQ(ellipsize("abc", 2), "...");
}

TEST(SelectBiome, Filter) {
    auto rows = sample();
    EXPECT_TRUE(matchesFilter(rows[0], ""));
    EXPECT_TRUE(matchesFilter(rows[0], "PLA"));
    EXPECT_FALSE(matchesFilter(rows[0], "desert"));
    // Display name (fixed), not the raw name.
    EXPECT_TRUE(matchesFilter(rows[3], "est hil"));
    EXPECT_FALSE(matchesFilter(rows[3], "ForestHills"));
    // $tags, @source
    EXPECT_TRUE(matchesFilter(rows[1], "$for"));
    EXPECT_FALSE(matchesFilter(rows[0], "$for"));
    EXPECT_TRUE(matchesFilter(rows[0], "$"));
    EXPECT_TRUE(matchesFilter(rows[3], "@ADD"));
    EXPECT_FALSE(matchesFilter(rows[0], "@add"));
}

TEST(SelectBiome, Sorting) {
    auto rows = sample();
    auto ids  = [&](BiomeSortMode mode) {
        std::vector<std::string> out;
        for (auto i : sortedRows(rows, mode)) out.push_back(rows[i].id);
        return out;
    };
    EXPECT_EQ(
        ids(BiomeSortMode::Name),
        (std::vector<std::string>{"minecraft:desert", "minecraft:forest", "addon:forest_hills", "minecraft:plains"})
    );
    // Source: "Minecraft" < "addon" (case-sensitive, as Java compareTo); ties keep name order.
    EXPECT_EQ(
        ids(BiomeSortMode::Source),
        (std::vector<std::string>{"minecraft:desert", "minecraft:forest", "minecraft:plains", "addon:forest_hills"})
    );
    // Dimension: "" first.
    EXPECT_EQ(ids(BiomeSortMode::Dimension).front(), "minecraft:desert");
    // Levels as strings: "0" < "10" < "2" < "3".
    EXPECT_EQ(
        ids(BiomeSortMode::Levels),
        (std::vector<std::string>{"minecraft:desert", "minecraft:forest", "minecraft:plains", "addon:forest_hills"})
    );
    EXPECT_EQ(
        ids(BiomeSortMode::Temperature),
        (std::vector<std::string>{"minecraft:forest", "addon:forest_hills", "minecraft:plains", "minecraft:desert"})
    );
    EXPECT_EQ(
        ids(BiomeSortMode::Rainfall),
        (std::vector<std::string>{"minecraft:desert", "minecraft:plains", "minecraft:forest", "addon:forest_hills"})
    );
    EXPECT_EQ(sortValue(rows[0], BiomeSortMode::Levels), "2");
    EXPECT_EQ(sortValue(rows[0], BiomeSortMode::Rainfall), "0.4");
    EXPECT_EQ(sortValue(rows[2], BiomeSortMode::Temperature), "2.0");
    rows[2].temperatureText = "2.00";
    EXPECT_EQ(sortValue(rows[2], BiomeSortMode::Temperature), "2.00"); // the catalog's text wins
}

TEST(SelectBiome, ButtonRules) {
    CompassView found{true, "minecraft:plains"};
    CompassView idle{};
    auto        s = buttonStates(std::nullopt, found, 25, true);
    EXPECT_FALSE(s.searchActive);
    EXPECT_TRUE(s.nextVisible);
    EXPECT_FALSE(s.nextActive);
    EXPECT_TRUE(s.teleportVisible);
    EXPECT_FALSE(s.teleportActive);

    s = buttonStates(std::string("minecraft:plains"), found, 25, true);
    EXPECT_TRUE(s.searchActive);
    EXPECT_TRUE(s.nextActive);
    EXPECT_TRUE(s.teleportActive);

    s = buttonStates(std::string("minecraft:forest"), found, 25, true);
    EXPECT_TRUE(s.searchActive);
    EXPECT_FALSE(s.nextActive);
    EXPECT_FALSE(s.teleportActive);

    s = buttonStates(std::string("minecraft:plains"), idle, 25, true);
    EXPECT_FALSE(s.nextActive);
    EXPECT_FALSE(s.teleportActive);

    s = buttonStates(std::string("minecraft:plains"), found, 0, false);
    EXPECT_FALSE(s.nextVisible);
    EXPECT_FALSE(s.nextActive);
    EXPECT_FALSE(s.teleportVisible);
    EXPECT_FALSE(s.teleportActive);
}

TEST(SelectBiome, ModelPreselectsFound) {
    SelectBiomeModel m(sample(), CompassView{true, "minecraft:forest"});
    ASSERT_TRUE(m.selected());
    EXPECT_EQ(*m.selected(), "minecraft:forest");
    EXPECT_EQ(m.sortMode(), BiomeSortMode::Name);
    EXPECT_EQ(m.filter(), "");
    EXPECT_EQ(m.listed().size(), 4u);

    SelectBiomeModel none(sample(), CompassView{false, "minecraft:forest"});
    EXPECT_FALSE(none.selected());
    SelectBiomeModel unknown(sample(), CompassView{true, "minecraft:nope"});
    EXPECT_FALSE(unknown.selected());
}

TEST(SelectBiome, ModelClickSelectsThenSearches) {
    SelectBiomeModel m(sample(), CompassView{});
    // Name order: desert, forest, forest_hills (unaffordable), plains.
    EXPECT_EQ(m.click(0), SelectBiomeModel::Click::Selected);
    EXPECT_EQ(*m.selected(), "minecraft:desert");
    EXPECT_EQ(m.click(3), SelectBiomeModel::Click::Selected);
    EXPECT_EQ(*m.selected(), "minecraft:plains");
    EXPECT_EQ(m.click(3), SelectBiomeModel::Click::Search);
    EXPECT_EQ(m.click(2), SelectBiomeModel::Click::Ignored); // can't afford
    EXPECT_EQ(*m.selected(), "minecraft:plains");
    EXPECT_EQ(m.click(99), SelectBiomeModel::Click::Ignored);
}

TEST(SelectBiome, ModelFilterAndSortRebuild) {
    SelectBiomeModel m(sample(), CompassView{true, "minecraft:forest"});
    EXPECT_FALSE(m.setFilter(""));
    EXPECT_TRUE(m.selected());
    EXPECT_TRUE(m.setFilter("for"));
    EXPECT_EQ(listedIds(m), (std::vector<std::string>{"minecraft:forest", "addon:forest_hills"}));
    EXPECT_FALSE(m.selected()); // rebuilt: selection dropped
    EXPECT_TRUE(m.setFilter("$desert"));
    EXPECT_EQ(listedIds(m), (std::vector<std::string>{"minecraft:desert"}));
    EXPECT_TRUE(m.setFilter("@addon"));
    EXPECT_EQ(listedIds(m), (std::vector<std::string>{"addon:forest_hills"}));
    EXPECT_TRUE(m.setFilter("zzz"));
    EXPECT_TRUE(m.listed().empty());
    EXPECT_EQ(m.listedRow(0), nullptr);

    EXPECT_TRUE(m.setFilter(""));
    m.click(0);
    m.cycleSort();
    EXPECT_EQ(m.sortMode(), BiomeSortMode::Source);
    EXPECT_FALSE(m.selected());
    EXPECT_EQ(listedIds(m).back(), "addon:forest_hills");

    m.select("minecraft:plains");
    EXPECT_EQ(*m.selected(), "minecraft:plains");
    m.select("minecraft:nope");
    EXPECT_FALSE(m.selected());
}

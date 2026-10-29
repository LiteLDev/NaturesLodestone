#include <gtest/gtest.h>

#include "core/HudLines.h"

using namespace nl::core::hud;

namespace {

Input base(State s) {
    Input in;
    in.state        = s;
    in.biomeName    = "Cherry Grove";
    in.foundX       = 300;
    in.foundZ       = -400;
    in.searchRadius = 1500;
    in.samples      = 4321;
    in.playerX      = 0;
    in.playerZ      = 0;
    return in;
}

} // namespace

TEST(HudLines, Inactive) { EXPECT_TRUE(buildLines(base(State::Inactive), {}).empty()); }

TEST(HudLines, Searching) {
    auto              lines = buildLines(base(State::Searching), {});
    std::vector<Line> want{
        {0, "Status",       false},
        {1, "Searching",    true },
        {3, "Biome",        false},
        {4, "Cherry Grove", true },
        {6, "Radius",       false},
        {7, "1500",         true },
    };
    EXPECT_EQ(lines, want);
}

TEST(HudLines, FoundWithCoordinates) {
    auto in                 = base(State::Found);
    in.displayCoords        = true;
    auto              lines = buildLines(in, {});
    std::vector<Line> want{
        {0,  "Status",       false},
        {1,  "Found",        true },
        {3,  "Biome",        false},
        {4,  "Cherry Grove", true },
        {6,  "Coordinates",  false},
        {7,  "300, -400",    true },
        {9,  "Distance",     false},
        {10, "500",          true },
    };
    EXPECT_EQ(lines, want);
}

TEST(HudLines, FoundWithoutCoordinates) {
    auto lines = buildLines(base(State::Found), {});
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines.back(), (Line{4, "Cherry Grove", true}));
}

TEST(HudLines, NotFound) {
    auto              lines = buildLines(base(State::NotFound), {});
    std::vector<Line> want{
        {0,  "Status",       false},
        {1,  "Not Found",    true },
        {3,  "Biome",        false},
        {4,  "Cherry Grove", true },
        {6,  "Radius",       false},
        {7,  "1500",         true },
        {9,  "Samples",      false},
        {10, "4321",         true },
    };
    EXPECT_EQ(lines, want);
}

TEST(HudLines, TranslatedLabels) {
    Labels zh;
    zh.status    = "状态";
    zh.searching = "搜索中";
    auto lines   = buildLines(base(State::Searching), zh);
    EXPECT_EQ(lines[0].text, "状态");
    EXPECT_EQ(lines[1].text, "搜索中");
}

TEST(HudLines, Distance) {
    EXPECT_EQ(horizontalDistance(0, 0, 3, 4), 5);
    EXPECT_EQ(horizontalDistance(10, 10, 10, 10), 0);
    EXPECT_EQ(horizontalDistance(0, 0, 1, 1), 1);    // 1.414 truncated
    EXPECT_EQ(horizontalDistance(-5, 7, 5, -7), 17); // sqrt(296) = 17.2
    EXPECT_EQ(horizontalDistance(-30000000, 0, 30000000, 0), 60000000);
}

TEST(HudLines, LineY) {
    EXPECT_EQ(lineY(0, 0), 7);
    EXPECT_EQ(lineY(0, 1), 16);
    EXPECT_EQ(lineY(10, 1), 106);
}

TEST(HudFallback, EncodeLayout) {
    auto lines = buildLines(base(State::Searching), {});
    auto s     = encodeFallback(lines, Side::Left, 1);
    EXPECT_EQ(s, std::string(kMarkerLeft) + "\n§fStatus\n§7Searching\n\n§fBiome\n§7Cherry Grove\n\n§fRadius\n§71500");
    EXPECT_TRUE(encodeFallback(lines, Side::Right, 0).starts_with(kMarkerRight));
}

TEST(HudFallback, RoundTrip) {
    for (auto st : {State::Searching, State::Found, State::NotFound}) {
        auto in          = base(st);
        in.displayCoords = true;
        auto lines       = buildLines(in, {});
        for (auto side : {Side::Left, Side::Right}) {
            for (int offset : {0, 1, 7, 50}) {
                auto d = decodeFallback(encodeFallback(lines, side, offset));
                ASSERT_TRUE(d.has_value());
                EXPECT_EQ(d->side, side);
                EXPECT_EQ(d->offset, offset);
                EXPECT_EQ(d->lines, lines);
            }
        }
    }
}

TEST(HudFallback, Clear) {
    auto s = encodeFallback({}, Side::Right, 3);
    EXPECT_EQ(s, std::string(kMarkerRight));
    auto d = decodeFallback(s);
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->side, Side::Right);
    EXPECT_TRUE(d->lines.empty());
}

TEST(HudFallback, Detection) {
    EXPECT_FALSE(decodeFallback("Hello").has_value());
    EXPECT_FALSE(decodeFallback("").has_value());
    EXPECT_FALSE(isFallback("Hello §r world"));
    EXPECT_TRUE(isFallback(encodeFallback({}, Side::Left, 0)));
    EXPECT_TRUE(isFallback("x" + encodeFallback({}, Side::Right, 0)));
    EXPECT_NE(kMarkerLeft, kMarkerRight);
    // Neither marker contains the other (the RP subtracts one marker from the text).
    EXPECT_EQ(std::string(kMarkerLeft).find(kMarkerRight), std::string::npos);
}

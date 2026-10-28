#include <gtest/gtest.h>

#include <set>

#include "core/SafeHeight.h"

using nl::core::durabilityBarDamage;
using nl::core::findTeleportY;
using nl::core::kFallbackTeleportY;

namespace {

/// A column where the heights in `solid` are not free.
struct Column {
    std::set<int> solid;
    bool          operator()(int y) const { return !solid.contains(y); }
};

Column filled(int from, int to) {
    Column c;
    for (int y = from; y <= to; ++y) c.solid.insert(y);
    return c;
}

} // namespace

TEST(SafeHeight, StartItselfIsChecked) {
    // ground at 62, air above: sea level 63 is valid at once
    EXPECT_EQ(findTeleportY(63, -64, 320, filled(-64, 62)), 63);
}

TEST(SafeHeight, GoesUpOutOfTheGround) {
    // ground up to 70: the up probe finds 71
    EXPECT_EQ(findTeleportY(63, -64, 320, filled(-64, 70)), 71);
}

TEST(SafeHeight, GoesDownIntoAValley) {
    // ground up to 50: the down probe finds 51
    EXPECT_EQ(findTeleportY(63, -64, 320, filled(-64, 50)), 51);
}

TEST(SafeHeight, UpWinsOverDownAtTheSameDistance) {
    // a pocket at 66/67 (floor 65) and one at 60/61 (floor 59), both 3 blocks away
    Column c = filled(40, 80);
    c.solid.erase(66);
    c.solid.erase(67);
    c.solid.erase(60);
    c.solid.erase(61);
    EXPECT_EQ(findTeleportY(63, -64, 320, c), 66);
}

TEST(SafeHeight, CloserProbeWins) {
    Column c = filled(40, 80);
    c.solid.erase(61); // floor 60, feet 61 needs head 62 too
    c.solid.erase(62);
    c.solid.erase(70);
    c.solid.erase(71);
    EXPECT_EQ(findTeleportY(63, -64, 320, c), 61);
}

TEST(SafeHeight, HeadMustBeFree) {
    // a one-block gap at 66 is not enough; the next valid spot is above the pillar
    Column c = filled(-64, 80);
    c.solid.erase(66);
    EXPECT_EQ(findTeleportY(63, -64, 320, c), 81);
}

TEST(SafeHeight, FallbackWhenNothingIsValid) {
    // all free: no floor anywhere
    EXPECT_EQ(findTeleportY(63, -64, 320, Column{}), kFallbackTeleportY);
    // all solid: no free space inside the build range
    EXPECT_EQ(findTeleportY(32, 0, 128, filled(0, 127)), 128); // the top of the build range (clamped)
    EXPECT_EQ(findTeleportY(32, 0, 128, Column{}), 128);       // 256 clamped to the build limit
}

TEST(SafeHeight, OutsideTheRangeIsFree) {
    // solid up to the last block: standing on top of the build limit is valid
    EXPECT_EQ(findTeleportY(120, 0, 128, filled(0, 127)), 128);
    // below the range is free: no floor there
    EXPECT_EQ(findTeleportY(2, 0, 128, filled(10, 127)), 128);
}

TEST(SafeHeight, DurabilityBar) {
    EXPECT_EQ(durabilityBarDamage(0, 10, 10000), 0);
    EXPECT_EQ(durabilityBarDamage(3, 0, 10000), 0);
    EXPECT_EQ(durabilityBarDamage(1, 4, 10000), 2500);
    EXPECT_EQ(durabilityBarDamage(4, 4, 10000), 9999); // broken: empty bar, never the full value
    EXPECT_EQ(durabilityBarDamage(9, 4, 10000), 9999);
    EXPECT_EQ(durabilityBarDamage(9999, 10000, 10000), 9999);
    EXPECT_EQ(durabilityBarDamage(1, 10000, 10000), 1);
    EXPECT_EQ(durabilityBarDamage(1, 100000, 10000), 1); // never 0 while damaged
    EXPECT_EQ(durabilityBarDamage(2, 3, 10000), 6667);
}

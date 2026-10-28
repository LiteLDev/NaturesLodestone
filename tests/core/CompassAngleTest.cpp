#include <gtest/gtest.h>

#include "core/CompassAngle.h"

using namespace nl::core;

TEST(CompassAngle, PositiveModulo) {
    EXPECT_DOUBLE_EQ(positiveModulo(0.25, 1.0), 0.25);
    EXPECT_DOUBLE_EQ(positiveModulo(1.25, 1.0), 0.25);
    EXPECT_DOUBLE_EQ(positiveModulo(-0.25, 1.0), 0.75);
    EXPECT_DOUBLE_EQ(positiveModulo(-1.0, 1.0), 0.0);
    double r = positiveModulo(-1e-18, 1.0);
    EXPECT_GE(r, 0.0);
    EXPECT_LT(r, 1.0);
}

// SPEC 3.8: 0 -> 16, 0.5 -> 17, ..., 14.5 -> 31, 15.5 -> 00, ..., 30.5 -> 15, 31.5 -> 16 (angle * 32).
TEST(CompassAngle, FrameMapping) {
    auto at = [](double scaled) { return needleFrame(scaled / 32.0); };
    EXPECT_EQ(at(0.0), 16);
    EXPECT_EQ(at(0.49), 16);
    EXPECT_EQ(at(0.5), 17);
    EXPECT_EQ(at(14.5), 31);
    EXPECT_EQ(at(15.49), 31);
    EXPECT_EQ(at(15.5), 0);
    EXPECT_EQ(at(16.0), 0);
    EXPECT_EQ(at(30.5), 15);
    EXPECT_EQ(at(31.49), 15);
    EXPECT_EQ(at(31.5), 16);
    EXPECT_EQ(at(31.99), 16);
    for (int i = 0; i < 32; ++i) {
        int frame = at(i + 0.25);
        EXPECT_GE(frame, 0);
        EXPECT_LT(frame, 32);
        EXPECT_EQ(frame, (i + 16) % 32) << "scaled " << i;
    }
    EXPECT_EQ(needleFrame(1.0), 16);
    EXPECT_EQ(needleFrame(-0.5), 0);
}

TEST(CompassAngle, AngleToTargetUsesBlockCentre) {
    EXPECT_NEAR(angleToTarget(0.5, 0.5, 10, 0), 0.0, 1e-12);   // east
    EXPECT_NEAR(angleToTarget(0.5, 0.5, 0, 10), 0.25, 1e-12);  // south
    EXPECT_NEAR(angleToTarget(0.5, 0.5, -10, 0), 0.5, 1e-12);  // west
    EXPECT_NEAR(angleToTarget(0.5, 0.5, 0, -10), -0.25, 1e-12); // north
}

TEST(CompassAngle, ValidTarget) {
    EXPECT_TRUE(isValidTarget(true, 1.0));
    EXPECT_FALSE(isValidTarget(false, 1.0));
    EXPECT_FALSE(isValidTarget(true, 0.0));
    EXPECT_FALSE(isValidTarget(true, 9e-6));
    EXPECT_TRUE(isValidTarget(true, 1e-5));
}

// Facing the target puts the needle straight up (frame 16), the target behind straight down (frame 0),
// on the right (clockwise) frame 24, on the left frame 8.
TEST(CompassAngle, StaticNeedleFollowsYaw) {
    // Target due north, holder at the origin.
    double north = angleToTarget(0.5, 0.5, 0, -100);
    EXPECT_EQ(needleFrame(staticNeedleAngle(north, 180)), 16); // facing north
    EXPECT_EQ(needleFrame(staticNeedleAngle(north, 0)), 0);    // facing south
    EXPECT_EQ(needleFrame(staticNeedleAngle(north, 90)), 24);  // facing west: north is on the right
    EXPECT_EQ(needleFrame(staticNeedleAngle(north, -90)), 8);  // facing east: north is on the left
    EXPECT_EQ(needleFrame(staticNeedleAngle(north, 270)), 8);
    // Target due east, facing east.
    double east = angleToTarget(0.5, 0.5, 100, 0);
    EXPECT_EQ(needleFrame(staticNeedleAngle(east, 270)), 16);
}

TEST(CompassAngle, SpinSeedOffset) {
    EXPECT_DOUBLE_EQ(spinSeedOffset(0), 0.0);
    // 1 * 1327217883 / 2147483650 (float division)
    EXPECT_NEAR(spinSeedOffset(1), 1327217883.0 / 2147483650.0, 1e-6);
    // 2 * 1327217883 overflows a Java int: 2654435766 - 2^32 = -1640531530
    EXPECT_NEAR(spinSeedOffset(2), -1640531530.0 / 2147483650.0, 1e-6);
}

TEST(CompassAngle, WobbleUpdatesOncePerTickAndConverges) {
    CompassWobble w;
    EXPECT_FALSE(w.shouldUpdate(0));
    EXPECT_TRUE(w.shouldUpdate(1));
    w.update(1, 0.3);
    EXPECT_FALSE(w.shouldUpdate(1));
    // delta = 0.3 * 0.1 * 0.8
    EXPECT_NEAR(w.rotation(), 0.024, 1e-12);
    for (std::int64_t t = 2; t < 400; ++t) w.update(t, 0.3);
    EXPECT_NEAR(w.rotation(), 0.3, 1e-6);
}

TEST(CompassAngle, WobbleTakesTheShortWayRound) {
    CompassWobble w;
    w.update(1, 0.9); // -0.1 the short way
    EXPECT_GT(w.rotation(), 0.5);
    EXPECT_NEAR(w.rotation(), 1.0 - 0.008, 1e-12);
}

TEST(CompassAngle, LocalPlayerNeedleSettlesOnStaticAngle) {
    Needle     n;
    NeedleView v;
    v.hasHolder     = true;
    v.isLocalPlayer = true;
    v.x             = 0.5;
    v.y             = 64;
    v.z             = 0.5;
    v.yaw           = 180; // facing north
    v.targetZ       = -100;
    double angle    = 0;
    for (std::int64_t t = 1; t < 400; ++t) {
        v.tick = t;
        angle  = n.angle(v, 0.0);
        // several renders in one tick do not move the needle further
        EXPECT_DOUBLE_EQ(n.angle(v, 0.0), angle);
    }
    EXPECT_NEAR(angle, staticNeedleAngle(angleToTarget(v.x, v.z, v.targetX, v.targetZ), v.yaw), 1e-6);
    EXPECT_EQ(needleFrame(angle), 16);
}

TEST(CompassAngle, OtherHoldersHaveNoWobble) {
    Needle     n;
    NeedleView v;
    v.hasHolder = true;
    v.x         = 0.5;
    v.z         = 0.5;
    v.yaw       = 90;
    v.targetZ   = -100;
    v.tick      = 1;
    EXPECT_EQ(n.frame(v, 0.0), 24);
}

TEST(CompassAngle, SpinsWithoutHolderOtherDimensionOrAtTarget) {
    NeedleView v;
    v.hasHolder = true;
    v.x         = 10.5;
    v.y         = 0.5;
    v.z         = 10.5;
    v.targetX   = 10;
    v.targetY   = 0;
    v.targetZ   = 10;
    v.yaw       = 0;
    v.seed      = 0;

    // At the target: the needle follows the spin wobble towards the random values.
    Needle at;
    v.tick = 1;
    EXPECT_TRUE(at.spinNeedsRandom(1));
    EXPECT_NEAR(at.angle(v, 0.3), 0.024, 1e-12); // 0.3 * 0.1 * 0.8
    EXPECT_FALSE(at.spinNeedsRandom(1));
    EXPECT_NEAR(at.angle(v, 0.9), 0.024, 1e-12); // same tick: the random value is not used
    EXPECT_TRUE(at.spinNeedsRandom(2));

    // Another dimension.
    Needle other;
    v.x             = 100;
    v.sameDimension = false;
    EXPECT_NEAR(other.angle(v, 0.3), 0.024, 1e-12);

    // No living holder (item frame / dropped).
    Needle none;
    v.sameDimension = true;
    v.hasHolder     = false;
    EXPECT_NEAR(none.angle(v, 0.3), 0.024, 1e-12);

    // The seed offsets the spin of one holder.
    Needle seeded;
    v.seed = 1;
    EXPECT_NEAR(seeded.angle(v, 0.3), positiveModulo(0.024 + spinSeedOffset(1), 1.0), 1e-6);
}

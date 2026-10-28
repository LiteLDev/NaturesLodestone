#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

#include "core/Spiral.h"

using nl::core::outFromOrigin;
using nl::core::SpiralPos;
using nl::core::SpiralSearch;

namespace {

using Event = SpiralSearch::Event;

SpiralSearch::Params params(int sampleSpace = 64, int maxRadius = 10000, int maxSamples = 50000) {
    SpiralSearch::Params p;
    p.sampleSpace = sampleSpace;
    p.maxRadius   = maxRadius;
    p.maxSamples  = maxSamples;
    p.yLevels     = {64};
    return p;
}

/// Runs the search to the end; returns the number of doWork() calls.
int runToEnd(SpiralSearch& s, SpiralSearch::Matcher const& m, int limit = 10'000'000) {
    int calls = 0;
    while (s.hasWork() && calls < limit) {
        s.doWork(m);
        calls++;
    }
    return calls;
}

} // namespace

TEST(Spiral, OutFromOriginOverworld) {
    EXPECT_EQ(outFromOrigin(64, -63, 319, 64), (std::vector<int>{64, 128, 0, 192, 256}));
    EXPECT_EQ(outFromOrigin(70, -63, 319, 64), (std::vector<int>{70, 134, 6, 198, -58, 262}));
    EXPECT_EQ(outFromOrigin(0, -63, 319, 64), (std::vector<int>{0, 64, 128, 192, 256}));
    EXPECT_EQ(outFromOrigin(-63, -63, 319, 64), (std::vector<int>{-63, 1, 65, 129, 193, 257}));
    EXPECT_EQ(outFromOrigin(319, -63, 319, 64), (std::vector<int>{319, 255, 191, 127, 63, -1}));
    EXPECT_EQ(outFromOrigin(200, -63, 319, 64), (std::vector<int>{200, 264, 136, 72, 8, -56}));
}

TEST(Spiral, OutFromOriginOtherDimensions) {
    // Nether (build height 0..128): only the start level fits.
    EXPECT_EQ(outFromOrigin(64, 1, 127, 64), (std::vector<int>{64}));
    EXPECT_EQ(outFromOrigin(32, 1, 127, 64), (std::vector<int>{32, 96}));
    // End (0..256).
    EXPECT_EQ(outFromOrigin(64, 1, 255, 64), (std::vector<int>{64, 128, 192}));
}

TEST(Spiral, OutFromOriginEdgeCases) {
    EXPECT_TRUE(outFromOrigin(-64, -63, 319, 64).empty()); // below the lowest level
    EXPECT_TRUE(outFromOrigin(320, -63, 319, 64).empty());
    EXPECT_TRUE(outFromOrigin(5, 10, 0, 64).empty());
    EXPECT_TRUE(outFromOrigin(5, 0, 10, 0).empty());
    EXPECT_EQ(outFromOrigin(5, 5, 5, 64), (std::vector<int>{5}));
    EXPECT_EQ(outFromOrigin(5, 0, 10, 1), (std::vector<int>{5, 6, 4, 7, 3, 8, 2, 9, 1, 10, 0}));
}

TEST(Spiral, QuartConversion) {
    EXPECT_EQ(nl::core::quartFromBlock(0), 0);
    EXPECT_EQ(nl::core::quartFromBlock(7), 1);
    EXPECT_EQ(nl::core::quartFromBlock(-1), -1);
    EXPECT_EQ(nl::core::quartFromBlock(-4), -1);
    EXPECT_EQ(nl::core::quartFromBlock(-5), -2);
    EXPECT_EQ(nl::core::blockFromQuart(-2), -8);
}

TEST(Spiral, PointSequence) {
    auto p   = params(64);
    p.startX = 10;
    p.startZ = -20;
    SpiralSearch                     s(p);
    std::vector<std::pair<int, int>> seen;
    for (int i = 0; i < 17; i++) {
        s.doWork([&](int x, int, int z) {
            seen.emplace_back((x - 10) / 64, (z + 20) / 64);
            return false;
        });
    }
    std::vector<std::pair<int, int>> expected{
        {0,  0 },
        {0,  -1},
        {1,  -1},
        {2,  -1},
        {2,  0 },
        {2,  1 },
        {2,  2 },
        {1,  2 },
        {0,  2 },
        {-1, 2 },
        {-2, 2 },
        {-2, 1 },
        {-2, 0 },
        {-2, -1},
        {-2, -2},
        {-2, -3},
        {-1, -3},
    };
    EXPECT_EQ(seen, expected);
    EXPECT_EQ(s.samples(), 17);
}

TEST(Spiral, LegLengths) {
    // Legs N1 E2 S3 W4 N5 E6 ...: count the points between two direction changes.
    nl::core::Spiral sp(0, 0, 1);
    sp.move(); // start
    sp.finishPoint();
    std::vector<std::pair<int, int>> pts;
    for (int i = 0; i < 1 + 2 + 3 + 4 + 5 + 6; i++) {
        sp.move();
        pts.emplace_back(sp.x(), sp.z());
        sp.finishPoint();
    }
    EXPECT_EQ(pts[0], (std::pair{0, -1}));   // N1
    EXPECT_EQ(pts[2], (std::pair{2, -1}));   // E2
    EXPECT_EQ(pts[5], (std::pair{2, 2}));    // S3
    EXPECT_EQ(pts[9], (std::pair{-2, 2}));   // W4
    EXPECT_EQ(pts[14], (std::pair{-2, -3})); // N5
    EXPECT_EQ(pts[20], (std::pair{4, -3}));  // E6
}

TEST(Spiral, YLevelOrderAndFirstMatch) {
    auto p    = params();
    p.yLevels = outFromOrigin(64, -63, 319, 64);
    SpiralSearch     s(p);
    std::vector<int> ys;
    auto             ev = s.doWork([&](int, int y, int) {
        ys.push_back(y);
        return y == 0 || y == 192;
    });
    EXPECT_EQ(ev, Event::Succeeded);
    EXPECT_EQ(ys, (std::vector<int>{64, 128, 0})); // stops at the first matching level
    EXPECT_EQ(s.found(), (SpiralPos{0, 0, 0}));
}

TEST(Spiral, MatchAtStart) {
    auto p   = params();
    p.startX = 100;
    p.startY = 70;
    p.startZ = -300;
    SpiralSearch s(p);
    EXPECT_TRUE(s.hasWork());
    EXPECT_EQ(s.doWork([](int, int, int) { return true; }), Event::Succeeded);
    EXPECT_TRUE(s.finished());
    EXPECT_FALSE(s.hasWork());
    EXPECT_EQ(s.samples(), 0);
    EXPECT_EQ(s.radius(), 0);
    EXPECT_EQ(s.found(), (SpiralPos{100, 64, -300}));
    ASSERT_EQ(s.prevPos().size(), 1u);
    EXPECT_EQ(s.prevPos()[0], (SpiralPos{100, 64, -300}));
}

TEST(Spiral, MatchAfterSomeSamples) {
    SpiralSearch s(params());
    // the biome is at the 5th point (2,0) * 64
    int calls = runToEnd(s, [](int x, int, int z) { return x == 128 && z == 0; });
    EXPECT_EQ(calls, 5);
    EXPECT_TRUE(s.finished());
    EXPECT_EQ(s.samples(), 4);
    EXPECT_EQ(s.found(), (SpiralPos{128, 64, 0}));
    EXPECT_EQ(s.radius(), 128);
}

TEST(Spiral, FailsAfterMaxSamplesPlusOne) {
    SpiralSearch s(params(64, 10000, 5));
    int          calls = runToEnd(s, [](int, int, int) { return false; });
    EXPECT_EQ(calls, 6); // samples <= maxSamples allows one more
    EXPECT_FALSE(s.finished());
    EXPECT_FALSE(s.hasWork());
    EXPECT_EQ(s.samples(), 6);
}

TEST(Spiral, ZeroMaxSamplesTakesOneSample) {
    SpiralSearch s(params(64, 10000, 0));
    EXPECT_EQ(runToEnd(s, [](int, int, int) { return false; }), 1);
    EXPECT_EQ(s.samples(), 1);
}

TEST(Spiral, FailsBeyondMaxRadius) {
    // (0,0) r 0, (0,-64) r 64 <= 64 so it goes on, (64,-64) r 90 is sampled, then it stops.
    SpiralSearch s(params(64, 64));
    int          calls = runToEnd(s, [](int, int, int) { return false; });
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(s.samples(), 3);
    EXPECT_EQ(s.radius(), 90);
}

TEST(Spiral, InvalidParametersFailAtOnce) {
    SpiralSearch a(params(0));
    EXPECT_FALSE(a.valid());
    EXPECT_FALSE(a.hasWork());
    SpiralSearch b(params(64, 0));
    EXPECT_FALSE(b.valid());
    EXPECT_FALSE(b.hasWork());
    EXPECT_EQ(b.doWork([](int, int, int) { return true; }), Event::None);
    EXPECT_EQ(b.samples(), 0);
}

TEST(Spiral, ProgressRadius) {
    SpiralSearch     s(params(200, 5000));
    std::vector<int> progress;
    std::vector<int> radiusAtProgress;
    while (s.hasWork()) {
        if (s.doWork([](int, int, int) { return false; }) == Event::Progress) {
            progress.push_back(s.progressRadius());
            radiusAtProgress.push_back(s.radius());
        }
    }
    ASSERT_FALSE(progress.empty());
    int last = 0;
    for (std::size_t i = 0; i < progress.size(); i++) {
        EXPECT_EQ(progress[i] % 500, 0);
        EXPECT_GT(progress[i], last);
        EXPECT_GT(radiusAtProgress[i], 500);
        EXPECT_EQ(progress[i], radiusAtProgress[i] / 500 * 500);
        last = progress[i];
    }
    EXPECT_EQ(progress.front(), 500);
    EXPECT_GE(s.radius(), 5000);
}

TEST(Spiral, NoProgressAtExactly500) {
    // ss 500: the second point is at radius 500 exactly (not > 500), the third at 707.
    SpiralSearch       s(params(500, 100000, 2));
    std::vector<Event> events;
    while (s.hasWork()) events.push_back(s.doWork([](int, int, int) { return false; }));
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0], Event::None);
    EXPECT_EQ(events[1], Event::None);
    EXPECT_EQ(events[2], Event::Progress);
    EXPECT_EQ(s.progressRadius(), 500);
}

namespace {

/// Two round patches of the biome, radius 100, around (0,0) and (cx,0); `bridge` joins them along z=0.
SpiralSearch::Matcher patches(int cx, bool bridge = false) {
    return [cx, bridge](int x, int, int z) {
        auto in = [](int x0, int z0, int x, int z) {
            double dx = x - x0, dz = z - z0;
            return std::sqrt(dx * dx + dz * dz) <= 100.0;
        };
        if (in(0, 0, x, z) || in(cx, 0, x, z)) return true;
        return bridge && x >= 0 && x <= cx && std::abs(z) <= 120;
    };
}

} // namespace

TEST(Spiral, NextSkipsConnectedCandidatesWithin2SampleSpaces) {
    auto p    = params();
    p.prevPos = {
        {0, 64, 0}
    };
    SpiralSearch s(p);
    // The start matches but lies within 128 of the previous position: skipped, counted.
    auto m = patches(100000);
    EXPECT_EQ(s.doWork(m), Event::None);
    EXPECT_TRUE(s.checking());
    EXPECT_EQ(s.doWork(m), Event::None); // init step: distance 0 <= 128 -> connected
    EXPECT_FALSE(s.checking());
    EXPECT_EQ(s.samples(), 1);
}

TEST(Spiral, NextFindsTheOtherPatch) {
    auto p    = params();
    p.prevPos = {
        {0, 64, 0}
    };
    SpiralSearch s(p);
    runToEnd(s, patches(640));
    ASSERT_TRUE(s.finished());
    auto f = s.found();
    EXPECT_LE(std::hypot(f.x - 640.0, f.z), 100.0);
    ASSERT_EQ(s.prevPos().size(), 2u);
    EXPECT_EQ(s.prevPos()[0], (SpiralPos{0, 64, 0}));
    EXPECT_EQ(s.prevPos()[1], f);
}

TEST(Spiral, NextSkipsPatchesJoinedByTheBiome) {
    auto p    = params(64, 1500);
    p.prevPos = {
        {0, 64, 0}
    };
    SpiralSearch s(p);
    runToEnd(s, patches(640, true));
    EXPECT_FALSE(s.finished()); // every match is connected to the first patch: the search fails
    EXPECT_FALSE(s.hasWork());
}

TEST(Spiral, NextLineCheckCostsOneWorkPerPoint) {
    // Previous position (0,0); the candidate is the start (640,0): distance 640 -> N = 10,
    // gap = clamp(10 / 4, 1, 10) = 2. Points x = 64, 128, ... ; the patch around (0,0) covers 64.
    auto p    = params();
    p.startX  = 640;
    p.prevPos = {
        {0, 64, 0}
    };
    SpiralSearch     s(p);
    std::vector<int> xs;
    auto             m = [&](int x, int, int z) {
        if (s.checking()) xs.push_back(x);
        return patches(640)(x, 0, z);
    };
    EXPECT_EQ(s.doWork(m), Event::None); // sample: match -> checking
    xs.clear();
    EXPECT_EQ(s.doWork(m), Event::None); // init step (no lookup)
    EXPECT_TRUE(xs.empty());
    EXPECT_EQ(s.doWork(m), Event::None);      // x=64: in the first patch
    EXPECT_EQ(s.doWork(m), Event::None);      // x=128: gap 1
    EXPECT_EQ(s.doWork(m), Event::Succeeded); // x=192: gap 2 -> not connected -> accepted
    EXPECT_EQ(xs, (std::vector<int>{64, 128, 192}));
    EXPECT_EQ(s.found(), (SpiralPos{640, 64, 0}));
    EXPECT_EQ(s.samples(), 0);
}

TEST(Spiral, NextLinePointsTruncateTowardZero) {
    // Previous (0,0), candidate (-300,-1000): t = i/N on the line, (int) truncation.
    auto p    = params(64);
    p.startX  = -300;
    p.startZ  = -1000;
    p.prevPos = {
        {0, 64, 0}
    };
    SpiralSearch                     s(p);
    std::vector<std::pair<int, int>> pts;
    auto                             m = [&](int x, int, int z) {
        if (s.checking()) pts.emplace_back(x, z);
        return !s.checking() || (x == 0 && z == 0); // only the candidate matches
    };
    s.doWork(m); // candidate
    s.doWork(m); // init: dist 1044.03 -> N = 16, gap 4
    for (int i = 0; i < 4; i++) s.doWork(m);
    EXPECT_TRUE(s.finished());
    double                           d = std::sqrt(300.0 * 300 + 1000.0 * 1000);
    int                              n = static_cast<int>(d / 64);
    std::vector<std::pair<int, int>> expected;
    for (int i = 1; i <= 4; i++) {
        double t = static_cast<double>(i) / n;
        expected.emplace_back(static_cast<int>(-300 * t), static_cast<int>(-1000 * t));
    }
    EXPECT_EQ(n, 16);
    EXPECT_EQ(pts, expected);
}

TEST(Spiral, NextMustBeDisconnectedFromEveryPrevious) {
    // Two previous positions: the candidate at (640,0) is far from the first (gap) but within
    // 128 of the second -> connected to the second -> skipped.
    auto p    = params(64, 64, 0);
    p.startX  = 640;
    p.prevPos = {
        {0,   64, 0},
        {700, 64, 0}
    };
    SpiralSearch s(p);
    auto         m = patches(640);
    runToEnd(s, m);
    EXPECT_FALSE(s.finished());
    EXPECT_EQ(s.samples(), 1);
}

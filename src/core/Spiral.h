#pragma once

// The game-independent part of the Nature's Compass biome search (docs/SPEC.md section 3.3):
// the y levels, the square spiral and the "Search for Next" connection check, driven one
// `doWork()` unit at a time. The game side (src/feature/search) supplies the biome test and
// runs the search within the per-tick budget.

#include <cstdint>
#include <functional>
#include <vector>

namespace nl::core {

/// NC's y levels (vanilla `Mth.outFromOrigin`): `origin` first, then alternately above and below
/// it in steps of `step`, every value within [lower, upper] (inclusive). Empty when `origin` is
/// outside the bounds, `lower > upper` or `step < 1`.
/// outFromOrigin(64, -63, 319, 64) = {64, 128, 0, 192, 256}.
[[nodiscard]] std::vector<int> outFromOrigin(int origin, int lower, int upper, int step);

/// Biome lookup resolution: block coordinate -> quart (4 blocks) coordinate (`>> 2`, floors).
[[nodiscard]] constexpr int quartFromBlock(int v) { return v >> 2; }
/// Quart coordinate -> its lowest block coordinate.
[[nodiscard]] constexpr int blockFromQuart(int q) { return q * 4; }

struct SpiralPos {
    int x = 0;
    int y = 0;
    int z = 0;

    friend bool operator==(SpiralPos const&, SpiralPos const&) = default;
};

/// NC's square spiral: the first point is the start, then legs north (-z), east (+x), south (+z),
/// west (-x), north, ... of 1, 2, 3, 4, 5, ... steps of `step` blocks.
/// Points (in steps): (0,0), (0,-1), (1,-1), (2,-1), (2,0), (2,1), (2,2), (1,2), (0,2), ...
class Spiral {
public:
    Spiral(int startX, int startZ, int step) : mX(startX), mZ(startZ), mStep(step), mNextLength(step) {}

    /// Moves to the next point (the first call stays on the start).
    void move();
    /// Ends the current point: advances the leg (NC finishSample's spiral part).
    void finishPoint();

    [[nodiscard]] int x() const { return mX; }
    [[nodiscard]] int z() const { return mZ; }

private:
    enum class Dir { Up, North, East, South, West };
    int mX;
    int mZ;
    int mStep;
    Dir mDir        = Dir::Up;
    int mLength     = 0;
    int mNextLength = 0;
};

/// One biome search (one NC BiomeSearchWorker without the item side).
class SpiralSearch {
public:
    struct Params {
        int startX = 0;
        int startY = 0;
        int startZ = 0;
        /// Blocks between two samples (sampleSpaceModifier * 4).
        int sampleSpace = 64;
        /// radiusModifier * 4.
        int maxRadius  = 10000;
        int maxSamples = 50000;
        /// `outFromOrigin(startY, minY + 1, maxY, 64)` of the searched dimension.
        std::vector<int> yLevels;
        /// Positions found by earlier searches (non-empty = Search for Next).
        std::vector<SpiralPos> prevPos;
    };

    /// What one `doWork()` produced.
    enum class Event {
        None,
        /// The radius passed a multiple of 500: write `progressRadius()` to the compass.
        Progress,
        /// Accepted a match: `found()`, `prevPos()` (with it appended), `samples()`.
        Succeeded,
    };

    /// `matches(x, y, z)`: the biome at that block position is the searched one.
    using Matcher = std::function<bool(int x, int y, int z)>;

    explicit SpiralSearch(Params params);

    /// The search can run at all (sampleSpace and maxRadius > 0); otherwise it fails at once.
    [[nodiscard]] bool valid() const { return mParams.sampleSpace > 0 && mParams.maxRadius > 0; }
    /// NC hasWork: valid, not finished, radius <= maxRadius and samples <= maxSamples. When this
    /// turns false without a success the search has failed (`radius()`, `samples()`).
    [[nodiscard]] bool hasWork() const;
    /// One unit of work: one sample column (every y level) or one connection-check step.
    Event doWork(Matcher const& matches);

    [[nodiscard]] bool finished() const { return mFinished; }
    /// (int) horizontal distance of the current spiral point from the start.
    [[nodiscard]] int radius() const;
    [[nodiscard]] int samples() const { return mSamples; }
    /// floor(radius / 500) * 500 at the last `Event::Progress`.
    [[nodiscard]] int progressRadius() const { return mLastThreshold * 500; }
    /// The accepted position (x/z of the sample, the matching y).
    [[nodiscard]] SpiralPos const&              found() const { return mFound; }
    [[nodiscard]] std::vector<SpiralPos> const& prevPos() const { return mParams.prevPos; }
    [[nodiscard]] Params const&                 params() const { return mParams; }
    /// A Search for Next candidate is being checked against the previous positions.
    [[nodiscard]] bool checking() const { return mChecking; }

private:
    /// First matching y of the column at x/z (nullopt-like: false when none).
    bool  matchColumn(Matcher const& matches, int x, int z, int& yOut) const;
    Event finishSample();
    Event succeed(SpiralPos pos);
    Event checkStep(Matcher const& matches);

    Params    mParams;
    Spiral    mSpiral;
    int       mSamples       = 0;
    int       mLastThreshold = 0;
    bool      mFinished      = false;
    SpiralPos mFound;

    // Search for Next connection check.
    bool        mChecking = false;
    SpiralPos   mCandidate;
    std::size_t mPrevIndex   = 0;
    int         mLineIndex   = 0; ///< 0 = not started for this previous position
    int         mLineCount   = 0; ///< N
    int         mConsecutive = 0;
};

} // namespace nl::core

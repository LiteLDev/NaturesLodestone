#include "core/Spiral.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace nl::core {

std::vector<int> outFromOrigin(int origin, int lower, int upper, int step) {
    std::vector<int> out;
    if (lower > upper || step < 1 || origin < lower || origin > upper) return out;
    std::int64_t const o = origin;
    std::int64_t       m = origin;
    // Each value is emitted while it (or its mirror) is within the bounds; the next value goes to
    // the other side of the origin when that side still has room, else further on the same side.
    while (true) {
        std::int64_t d = std::llabs(o - m);
        if (!(o - d >= lower || o + d <= upper)) break;
        out.push_back(static_cast<int>(m));
        bool         atOrBelow = m <= o;
        bool         upFits    = o + d + step <= upper;
        std::int64_t next      = o + d + step;
        if (!atOrBelow || !upFits) {
            std::int64_t down = o - d - (atOrBelow ? step : 0);
            if (down >= lower) next = down;
        }
        m = next;
    }
    return out;
}

// ---- Spiral --------------------------------------------------------------------------------------

void Spiral::move() {
    switch (mDir) {
    case Dir::North:
        mZ -= mStep;
        break;
    case Dir::East:
        mX += mStep;
        break;
    case Dir::South:
        mZ += mStep;
        break;
    case Dir::West:
        mX -= mStep;
        break;
    case Dir::Up:
        break;
    }
}

void Spiral::finishPoint() {
    mLength += mStep;
    if (mLength < mNextLength) return;
    switch (mDir) {
    case Dir::Up:
        mDir = Dir::North;
        break; // the start point: the first leg keeps length 1
    case Dir::North:
        mDir         = Dir::East;
        mNextLength += mStep;
        break;
    case Dir::East:
        mDir         = Dir::South;
        mNextLength += mStep;
        break;
    case Dir::South:
        mDir         = Dir::West;
        mNextLength += mStep;
        break;
    case Dir::West:
        mDir         = Dir::North;
        mNextLength += mStep;
        break;
    }
    mLength = 0;
}

// ---- SpiralSearch --------------------------------------------------------------------------------

SpiralSearch::SpiralSearch(Params params)
: mParams(std::move(params)),
  mSpiral(mParams.startX, mParams.startZ, mParams.sampleSpace) {}

int SpiralSearch::radius() const {
    double dx = static_cast<double>(mSpiral.x()) - mParams.startX;
    double dz = static_cast<double>(mSpiral.z()) - mParams.startZ;
    // NC: (int) Mth.sqrt((float) distSqr)
    return static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dz * dz)));
}

bool SpiralSearch::hasWork() const {
    return valid() && !mFinished && radius() <= mParams.maxRadius && mSamples <= mParams.maxSamples;
}

bool SpiralSearch::matchColumn(Matcher const& matches, int x, int z, int& yOut) const {
    for (int y : mParams.yLevels) {
        if (matches(x, y, z)) {
            yOut = y;
            return true;
        }
    }
    return false;
}

SpiralSearch::Event SpiralSearch::finishSample() {
    mSamples++;
    mSpiral.finishPoint();
    int r = radius();
    if (r > 500 && r / 500 > mLastThreshold) {
        mLastThreshold = r / 500;
        return Event::Progress;
    }
    return Event::None;
}

SpiralSearch::Event SpiralSearch::succeed(SpiralPos pos) {
    mFound = pos;
    mParams.prevPos.push_back(pos);
    mFinished = true;
    mChecking = false;
    return Event::Succeeded;
}

SpiralSearch::Event SpiralSearch::doWork(Matcher const& matches) {
    if (!hasWork()) return Event::None;
    if (mChecking) return checkStep(matches);

    mSpiral.move();
    int y = 0;
    if (matchColumn(matches, mSpiral.x(), mSpiral.z(), y)) {
        SpiralPos candidate{mSpiral.x(), y, mSpiral.z()};
        if (mParams.prevPos.empty()) return succeed(candidate);
        mChecking    = true;
        mCandidate   = candidate;
        mPrevIndex   = 0;
        mLineIndex   = 0;
        mLineCount   = 0;
        mConsecutive = 0;
        return Event::None;
    }
    return finishSample();
}

SpiralSearch::Event SpiralSearch::checkStep(Matcher const& matches) {
    // Not connected to any previous position: a new instance of the biome.
    if (mPrevIndex >= mParams.prevPos.size()) return succeed(mCandidate);

    SpiralPos const& prev = mParams.prevPos[mPrevIndex];
    double const     dx   = static_cast<double>(mCandidate.x) - prev.x;
    double const     dz   = static_cast<double>(mCandidate.z) - prev.z;

    if (mLineIndex == 0) {
        double dist = std::sqrt(dx * dx + dz * dz);
        if (dist <= 2.0 * mParams.sampleSpace) {
            mChecking = false; // connected: skip the candidate
            return finishSample();
        }
        mLineCount   = static_cast<int>(dist / mParams.sampleSpace);
        mLineIndex   = 1;
        mConsecutive = 0;
        return Event::None;
    }

    if (mLineIndex >= mLineCount) {
        mChecking = false; // the whole line is the biome: connected, skip the candidate
        return finishSample();
    }

    double t  = static_cast<double>(mLineIndex) / mLineCount;
    int    px = static_cast<int>(prev.x + dx * t);
    int    pz = static_cast<int>(prev.z + dz * t);
    int    y  = 0;
    if (matchColumn(matches, px, pz, y)) {
        mConsecutive = 0;
    } else {
        mConsecutive++;
    }
    int gap = std::clamp(mLineCount / 4, 1, 10);
    if (mConsecutive >= gap) {
        // A gap: the candidate is not connected to this previous position.
        mPrevIndex++;
        mLineIndex = 0;
        if (mPrevIndex >= mParams.prevPos.size()) return succeed(mCandidate);
        return Event::None;
    }
    mLineIndex++;
    return Event::None;
}

} // namespace nl::core

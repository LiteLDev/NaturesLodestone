#pragma once

// Compass needle angle (docs/SPEC.md section 3.8): the vanilla Java compass logic that Nature's
// Compass uses for its needle, as pure math. Angles are in turns (0..1) clockwise on the icon:
// 0 = the needle points straight up (frame 16, the holder faces the target), 0.25 = right
// (frame 24), 0.5 = straight down (frame 00), 0.75 = left (frame 08).
//
// Coordinates and yaw follow Minecraft (Java and Bedrock agree): +x east, +z south, yaw 0 faces
// south and grows clockwise seen from above (90 = west).

#include <cstdint>

namespace nl::core {

/// Number of needle frames (`naturescompass_00` .. `_31`).
inline constexpr int kNeedleFrames = 32;

/// `v` modulo `m` in [0, m) (Java `Mth.positiveModulo`).
[[nodiscard]] double positiveModulo(double v, double m);

/// The needle frame for `angle` (turns), as NC's model dispatches it: angle * 32 against the
/// thresholds 0 -> 16, 0.5 -> 17, ..., 15.5 -> 00, ..., 31.5 -> 16.
[[nodiscard]] int needleFrame(double angle);

/// Direction from (fromX, fromZ) to the centre of block (targetX, targetZ): atan2(dz, dx) / 2π.
[[nodiscard]] double angleToTarget(double fromX, double fromZ, int targetX, int targetZ);

/// A compass target is usable when it is in the holder's dimension and the holder is not (almost)
/// exactly at the target block's centre (squared distance < 1e-5; vanilla `isValidCompassTargetPos`).
[[nodiscard]] bool isValidTarget(bool sameDimension, double distSqToTargetCentre);

/// The holder's yaw as a fraction of a turn in [0, 1).
[[nodiscard]] double wrappedYaw(double yawDegrees);

/// Needle angle for a holder that is not the local player: 0.5 - (yaw - 0.25 - angle), no wobble.
[[nodiscard]] double staticNeedleAngle(double angleToTarget, double yawDegrees);

/// What the local player's wobble eases towards: 0.5 - (yaw - 0.25). The needle angle is then
/// angleToTarget + the wobble's rotation.
[[nodiscard]] double localWobbleTarget(double yawDegrees);

/// The spin offset of one holder: (float)(seed * 1327217883) / 2.14748365e9 (Java int overflow).
[[nodiscard]] double spinSeedOffset(int seed);

/// Vanilla `CompassWobble`: a damped spring that eases a rotation towards a target, updated at
/// most once per game tick (delta += diff * 0.1; delta *= 0.8).
class CompassWobble {
public:
    [[nodiscard]] double rotation() const { return mRotation; }
    [[nodiscard]] bool   shouldUpdate(std::int64_t tick) const { return mLastTick != tick; }
    void                 update(std::int64_t tick, double target);
    void                 reset() { *this = CompassWobble{}; }

private:
    double       mRotation = 0;
    double       mDelta    = 0;
    std::int64_t mLastTick = 0;
};

/// Everything the needle depends on for one rendered stack.
struct NeedleView {
    /// There is a living holder (false: item frame, dropped item, no entity).
    bool hasHolder = false;
    /// The holder is the local player (its needle wobbles).
    bool isLocalPlayer = false;
    /// The target is in the holder's dimension.
    bool   sameDimension = true;
    double x = 0, y = 0, z = 0; ///< holder position (feet)
    double yaw = 0;             ///< holder yaw, degrees
    int    targetX = 0, targetY = 0, targetZ = 0;
    int    seed = 0;            ///< per-holder spin offset (entity id)
    std::int64_t tick = 0;      ///< game tick (wobble updates once per tick)
};

/// The needle of one item type: the local-player wobble and the random-spin wobble are shared by
/// every stack of the item (as the per-item property function in Java).
class Needle {
public:
    /// The needle angle (turns, [0, 1)). `random01` is a fresh uniform [0, 1) value; it is used
    /// only when the spin wobble picks a new target this tick.
    [[nodiscard]] double angle(NeedleView const& view, double random01);
    /// `needleFrame(angle(view, random01))`.
    [[nodiscard]] int frame(NeedleView const& view, double random01) { return needleFrame(angle(view, random01)); }
    /// True when `angle` at `tick` would use `random01` (the spin wobble has not moved this tick),
    /// so callers can skip drawing a random number otherwise.
    [[nodiscard]] bool spinNeedsRandom(std::int64_t tick) const { return mSpin.shouldUpdate(tick); }
    void               reset();

private:
    CompassWobble mWobble;
    CompassWobble mSpin;
};

} // namespace nl::core

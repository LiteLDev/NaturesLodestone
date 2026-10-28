#include "core/CompassAngle.h"

#include <cmath>
#include <numbers>

namespace nl::core {

double positiveModulo(double v, double m) {
    double r = std::fmod(v, m);
    if (r < 0) r += m;
    return r >= m ? 0.0 : r; // fmod of a tiny negative value + m can round up to m
}

int needleFrame(double angle) {
    double value = positiveModulo(angle, 1.0) * kNeedleFrames;
    int    step  = static_cast<int>(std::floor(value + 0.5)); // 0 .. 32
    return (step + kNeedleFrames / 2) % kNeedleFrames;
}

double angleToTarget(double fromX, double fromZ, int targetX, int targetZ) {
    double dx = targetX + 0.5 - fromX;
    double dz = targetZ + 0.5 - fromZ;
    return std::atan2(dz, dx) / (2.0 * std::numbers::pi);
}

bool isValidTarget(bool sameDimension, double distSqToTargetCentre) {
    return sameDimension && !(distSqToTargetCentre < 1.0e-5);
}

double wrappedYaw(double yawDegrees) { return positiveModulo(yawDegrees / 360.0, 1.0); }

double staticNeedleAngle(double angleToTarget, double yawDegrees) {
    return positiveModulo(0.5 - (wrappedYaw(yawDegrees) - 0.25 - angleToTarget), 1.0);
}

double localWobbleTarget(double yawDegrees) { return 0.5 - (wrappedYaw(yawDegrees) - 0.25); }

double spinSeedOffset(int seed) {
    auto hashed = static_cast<std::int32_t>(static_cast<std::uint32_t>(seed) * 1327217883u);
    return static_cast<double>(static_cast<float>(hashed) / 2.14748365e9f);
}

void CompassWobble::update(std::int64_t tick, double target) {
    mLastTick   = tick;
    double diff = positiveModulo(target - mRotation + 0.5, 1.0) - 0.5;
    mDelta += diff * 0.1;
    mDelta *= 0.8;
    mRotation = positiveModulo(mRotation + mDelta, 1.0);
}

double Needle::angle(NeedleView const& v, double random01) {
    bool valid = false;
    if (v.hasHolder) {
        double dx = v.targetX + 0.5 - v.x;
        double dy = v.targetY + 0.5 - v.y;
        double dz = v.targetZ + 0.5 - v.z;
        valid     = isValidTarget(v.sameDimension, dx * dx + dy * dy + dz * dz);
    }
    if (!valid) {
        if (mSpin.shouldUpdate(v.tick)) mSpin.update(v.tick, random01);
        return positiveModulo(mSpin.rotation() + spinSeedOffset(v.seed), 1.0);
    }
    double toTarget = angleToTarget(v.x, v.z, v.targetX, v.targetZ);
    if (v.isLocalPlayer) {
        if (mWobble.shouldUpdate(v.tick)) mWobble.update(v.tick, localWobbleTarget(v.yaw));
        return positiveModulo(toTarget + mWobble.rotation(), 1.0);
    }
    return staticNeedleAngle(toTarget, v.yaw);
}

void Needle::reset() {
    mWobble.reset();
    mSpin.reset();
}

} // namespace nl::core

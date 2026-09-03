#include "pch.h"
#include "FlightDynamics.h"

namespace ParagliderVR
{
    namespace
    {
        float MoveTowards(float a_current, float a_target, float a_maximumDelta)
        {
            const float difference = a_target - a_current;
            if (std::abs(difference) <= a_maximumDelta) {
                return a_target;
            }
            return a_current + std::copysign(a_maximumDelta, difference);
        }
    }

    RE::NiPoint3 CalculateVelocityDelta(
        const FlightCommand& a_command,
        const RE::NiPoint3& a_baseVelocity)
    {
        if (!a_command.active) {
            return {};
        }

        RE::NiPoint3 target = a_baseVelocity;
        const float maximumVerticalChange =
            a_command.verticalTransitionAcceleration * a_command.deltaTime;
        target.z = MoveTowards(
            a_baseVelocity.z,
            a_command.verticalTargetSpeed,
            maximumVerticalChange);

        if (a_command.steeringDirection.Length() > 0.001f) {
            const auto desiredHorizontal =
                (a_command.steeringDirection * a_command.steeringSpeed) +
                (a_command.lateralDirection * a_command.lateralSpeed);
            RE::NiPoint3 horizontalDifference{
                desiredHorizontal.x - target.x,
                desiredHorizontal.y - target.y,
                0.0f
            };
            const float currentHorizontalSpeed =
                std::sqrt((target.x * target.x) + (target.y * target.y));
            const float desiredHorizontalSpeed = desiredHorizontal.Length();
            const float rate = desiredHorizontalSpeed >= currentHorizontalSpeed ?
                a_command.horizontalAcceleration : a_command.horizontalDeceleration;
            const float maximumHorizontalChange = rate * a_command.deltaTime;
            const float differenceLength = horizontalDifference.Length();
            if (differenceLength > maximumHorizontalChange && differenceLength > 0.0001f) {
                horizontalDifference =
                    horizontalDifference / differenceLength * maximumHorizontalChange;
            }
            target.x += horizontalDifference.x;
            target.y += horizontalDifference.y;
        }

        return target - a_baseVelocity;
    }
}

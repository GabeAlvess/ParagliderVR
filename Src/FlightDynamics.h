#pragma once

namespace ParagliderVR
{
    struct FlightCommand
    {
        bool active = false;
        float verticalTargetSpeed = 0.0f;
        float steeringSpeed = 0.0f;
        float lateralSpeed = 0.0f;
        float horizontalAcceleration = 0.0f;
        float horizontalDeceleration = 0.0f;
        float verticalTransitionAcceleration = 0.0f;
        float deltaTime = 0.0f;
        RE::NiPoint3 steeringDirection{};
        RE::NiPoint3 lateralDirection{};
    };

    [[nodiscard]] RE::NiPoint3 CalculateVelocityDelta(
        const FlightCommand& a_command,
        const RE::NiPoint3& a_baseVelocity);
}

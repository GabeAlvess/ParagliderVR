#include "pch.h"
#include "Config.h"
#include "FlightControl.h"

namespace ParagliderVR
{
    namespace
    {
        float ApplyDeadzone(float a_value, float a_deadzone)
        {
            const float magnitude = std::abs(a_value);
            if (magnitude <= a_deadzone) {
                return 0.0f;
            }
            return std::copysign(
                (magnitude - a_deadzone) / (1.0f - a_deadzone),
                a_value);
        }

        RE::NiPoint3 Normalize(const RE::NiPoint3& a_value, const RE::NiPoint3& a_fallback = {})
        {
            const float length = a_value.Length();
            return length > 0.0001f ? a_value / length : a_fallback;
        }

        RE::NiPoint3 HmdForward(const RE::NiAVObject& a_hmd)
        {
            return Normalize({
                a_hmd.world.rotate.entry[0][1],
                a_hmd.world.rotate.entry[1][1],
                0.0f
            }, { 0.0f, 1.0f, 0.0f });
        }

        RE::NiPoint3 HmdRight(const RE::NiPoint3& a_forward)
        {
            return Normalize({ a_forward.y, -a_forward.x, 0.0f }, { 1.0f, 0.0f, 0.0f });
        }
    }

    void FlightControl::Begin(RE::PlayerCharacter* a_player, const Settings& a_settings)
    {
        float initialHorizontalSpeed = a_settings.minimumForwardSpeed;
        if (a_player) {
            if (auto* controller = a_player->GetCharController()) {
                const float worldScale = RE::bhkWorld::GetWorldScale();
                if (std::isfinite(worldScale) && worldScale > 0.000001f) {
                    RE::hkVector4 velocity;
                    controller->GetLinearVelocityImpl(velocity);
                    const float x = velocity.quad.m128_f32[0] / worldScale;
                    const float y = velocity.quad.m128_f32[1] / worldScale;
                    initialHorizontalSpeed = std::sqrt((x * x) + (y * y));
                }
            }
        }
        _commandedHorizontalSpeed = std::clamp(
            initialHorizontalSpeed,
            a_settings.minimumForwardSpeed,
            a_settings.steeringSpeed);
    }

    void FlightControl::Reset()
    {
        _commandedHorizontalSpeed = 0.0f;
    }

    FlightControlSample FlightControl::BuildCommand(
        const RE::NiAVObject& a_hmd,
        const ParagliderInputState& a_input,
        bool a_dualHanded,
        bool a_staminaExhausted,
        float a_delta,
        const Settings& a_settings)
    {
        FlightControlSample sample{};
        sample.dominantThrottle = ApplyDeadzone(
            a_input.mainThumbstick.y,
            a_settings.thumbstickDeadzone);
        sample.offhandThrottle = ApplyDeadzone(
            a_input.offThumbstick.y,
            a_settings.thumbstickDeadzone);
        sample.lateralThrottle = ApplyDeadzone(
            a_input.offThumbstick.x,
            a_settings.thumbstickDeadzone);

        if (sample.offhandThrottle > 0.0f) {
            _commandedHorizontalSpeed +=
                a_settings.horizontalAcceleration * sample.offhandThrottle * a_delta;
        } else if (sample.offhandThrottle < 0.0f) {
            _commandedHorizontalSpeed +=
                a_settings.horizontalDeceleration * sample.offhandThrottle * a_delta;
        }
        _commandedHorizontalSpeed = std::clamp(
            _commandedHorizontalSpeed,
            a_settings.minimumForwardSpeed,
            a_settings.steeringSpeed);
        sample.commandedHorizontalSpeed = _commandedHorizontalSpeed;

        auto& command = sample.command;
        command.active = true;
        const float gentleFallSpeed =
            -a_settings.referenceFallSpeed * a_settings.dualMinimumFallMultiplier;
        const float maximumDescentSpeed =
            -a_settings.referenceFallSpeed * a_settings.dualMaximumFallMultiplier;
        if (!a_dualHanded) {
            command.verticalTargetSpeed =
                -a_settings.referenceFallSpeed * a_settings.singleFallMultiplier;
        } else if (sample.dominantThrottle >= 0.0f) {
            command.verticalTargetSpeed = gentleFallSpeed +
                ((a_settings.maximumClimbSpeed - gentleFallSpeed) * sample.dominantThrottle);
        } else {
            command.verticalTargetSpeed = gentleFallSpeed +
                ((maximumDescentSpeed - gentleFallSpeed) * -sample.dominantThrottle);
        }
        if (a_staminaExhausted) {
            command.verticalTargetSpeed =
                -a_settings.referenceFallSpeed * a_settings.exhaustedFallMultiplier;
        }

        command.steeringDirection = HmdForward(a_hmd);
        command.steeringSpeed = _commandedHorizontalSpeed *
            (a_staminaExhausted ? a_settings.exhaustedHorizontalSpeedScale : 1.0f);
        command.lateralDirection = HmdRight(command.steeringDirection);
        command.lateralSpeed =
            _commandedHorizontalSpeed * a_settings.lateralSpeedScale * sample.lateralThrottle *
            (a_staminaExhausted ? a_settings.exhaustedHorizontalSpeedScale : 1.0f);
        command.horizontalAcceleration = a_settings.horizontalAcceleration;
        command.horizontalDeceleration = a_settings.horizontalDeceleration;
        command.verticalTransitionAcceleration = a_settings.verticalTransitionAcceleration;
        command.deltaTime = a_delta;
        return sample;
    }

    float FlightControl::GetCommandedHorizontalSpeed() const
    {
        return _commandedHorizontalSpeed;
    }
}

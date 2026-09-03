#pragma once

#include "FlightDynamics.h"
#include "ParagliderInput.h"

namespace ParagliderVR
{
    struct Settings;

    struct FlightControlSample
    {
        FlightCommand command{};
        float dominantThrottle = 0.0f;
        float offhandThrottle = 0.0f;
        float lateralThrottle = 0.0f;
        float commandedHorizontalSpeed = 0.0f;
    };

    class FlightControl final
    {
    public:
        void Begin(RE::PlayerCharacter* a_player, const Settings& a_settings);
        void Reset();
        [[nodiscard]] FlightControlSample BuildCommand(
            const RE::NiAVObject& a_hmd,
            const ParagliderInputState& a_input,
            bool a_dualHanded,
            bool a_staminaExhausted,
            float a_delta,
            const Settings& a_settings);
        [[nodiscard]] float GetCommandedHorizontalSpeed() const;

    private:
        float _commandedHorizontalSpeed = 0.0f;
    };
}

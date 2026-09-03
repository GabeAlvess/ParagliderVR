#pragma once

#include "FlightDynamics.h"
#include "FlightStamina.h"
#include "WindVisualController.h"

namespace ParagliderVR
{
    struct Settings;

    class FlightSession final
    {
    public:
        void Start(RE::PlayerCharacter* a_player, float a_initialHorizontalSpeed);
        void Stop(bool a_wasActive, std::string_view a_reason);
        [[nodiscard]] bool Update(
            RE::PlayerCharacter& a_player,
            float a_delta,
            const Settings& a_settings);
        void UpdateAnimationDirection(float a_offhandThrottle);
        void UpdateWind(RE::PlayerCharacter& a_player, const FlightCommand& a_command);

    private:
        FlightStamina _stamina;
        WindVisualController _windVisual;
        float _animationRefreshTimer = 0.0f;
    };
}

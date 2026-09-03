#pragma once

#include "FlightDynamics.h"
#include "FlightControl.h"
#include "FireLiftController.h"
#include "FlightSession.h"
#include "FlightSafety.h"
#include "ParagliderActivation.h"
#include "ParagliderInput.h"

#include <mutex>

namespace ParagliderVR
{
    class ParagliderController final
    {
    public:
        static ParagliderController& GetSingleton();
        void SetEnabled(bool a_enabled);
        void Update();
        [[nodiscard]] bool IsFlightActive() const;
        [[nodiscard]] FlightCommand GetFlightCommand() const;

    private:
        void StartFlight();
        void StopFlight(std::string_view a_reason);
        void UpdateFlight(
            RE::PlayerCharacter& a_player,
            float a_delta,
            const ParagliderInputState& a_input);
        mutable std::mutex _flightLock;
        FlightCommand _flight{};
        FlightControl _flightControl;
        FireLiftController _fireLift;
        FlightSession _flightSession;
        FlightSafety _flightSafety;
        ParagliderActivation _activation;
        bool _enabled = false;
        float _gestureLogTimer = 0.0f;
    };
}

#pragma once

#include "ParagliderInput.h"

namespace ParagliderVR
{
    class PhysicalParagliderController;

    class ParagliderCalibration final
    {
    public:
        void OnDeployed();
        void Update(
            RE::PlayerCharacter& a_player,
            float a_delta,
            const ParagliderInputState& a_input,
            PhysicalParagliderController& a_physical);
        void Reset();

    private:
        static constexpr float kHoldSeconds = 5.0f;

        std::array<float, 2> _holdSeconds{};
        std::array<int, 2> _loggedSeconds{};
        bool _captured = false;
        bool _awaitingGripRelease = false;
    };
}

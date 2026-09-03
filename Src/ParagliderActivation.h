#pragma once

#include "ParagliderCalibration.h"
#include "ParagliderInput.h"

#include <array>

namespace ParagliderVR
{
    class PhysicalParagliderController;

    class ParagliderActivation final
    {
    public:
        void ResetCalibration();
        void BlockUntilGripRelease();
        [[nodiscard]] bool IsBlocked(const ParagliderInputState& a_input);
        [[nodiscard]] bool Update(
            RE::PlayerCharacter& a_player,
            float a_delta,
            const ParagliderInputState& a_input,
            PhysicalParagliderController& a_physical);
    private:
        ParagliderCalibration _calibration;
        std::array<bool, 2> _gripPressedInsideActivationArea{};
        bool _blockedUntilGripRelease = false;
    };
}

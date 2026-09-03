#pragma once

#include "ParagliderInput.h"

namespace ParagliderVR
{
    struct Settings;
    class PhysicalParagliderController;

    enum class FlightSafetyStatus
    {
        kContinue,
        kWaitForHands,
        kStop
    };

    struct FlightSafetyResult
    {
        FlightSafetyStatus status = FlightSafetyStatus::kContinue;
        std::string_view stopReason{};
        RE::NiAVObject* hmd = nullptr;
        int gripCount = 0;
        int heldHandCount = 0;
    };

    class FlightSafety final
    {
    public:
        void Reset();
        [[nodiscard]] FlightSafetyResult Evaluate(
            RE::PlayerCharacter& a_player,
            float a_delta,
            const ParagliderInputState& a_input,
            const PhysicalParagliderController& a_physical,
            const Settings& a_settings);
        [[nodiscard]] static bool IsAirborne(const RE::PlayerCharacter& a_player);

    private:
        float _physicalReleaseGraceTimer = 0.0f;
    };
}

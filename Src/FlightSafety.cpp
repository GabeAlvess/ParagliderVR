#include "pch.h"
#include "Config.h"
#include "FlightSafety.h"
#include "ParagliderBallisticController.h"
#include "PhysicalParagliderController.h"

namespace ParagliderVR
{
    namespace
    {
        constexpr float kPhysicalReleaseGraceSeconds = 0.05f;
    }

    void FlightSafety::Reset()
    {
        _physicalReleaseGraceTimer = 0.0f;
    }

    FlightSafetyResult FlightSafety::Evaluate(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const ParagliderInputState& a_input,
        const PhysicalParagliderController& a_physical,
        const Settings& a_settings)
    {
        FlightSafetyResult result{};
        if (!ParagliderBallisticController::GetSingleton().IsInFlight() &&
            !IsAirborne(a_player)) {
            result.status = FlightSafetyStatus::kStop;
            result.stopReason = "landed";
            return result;
        }

        result.gripCount = static_cast<int>(a_input.hands[0].gripDown) +
            static_cast<int>(a_input.hands[1].gripDown);
        result.heldHandCount = static_cast<int>(a_physical.IsHeldByHand(true)) +
            static_cast<int>(a_physical.IsHeldByHand(false));
        if (result.heldHandCount == 0) {
            _physicalReleaseGraceTimer += a_delta;
            if (_physicalReleaseGraceTimer >= kPhysicalReleaseGraceSeconds) {
                result.status = FlightSafetyStatus::kStop;
                result.stopReason = "both HIGGS hands released";
            } else {
                result.status = FlightSafetyStatus::kWaitForHands;
            }
            return result;
        }
        _physicalReleaseGraceTimer = 0.0f;

        auto* vrData = a_player.GetVRNodeData();
        result.hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
        if (!result.hmd) {
            result.status = FlightSafetyStatus::kStop;
            result.stopReason = "HMD unavailable";
            return result;
        }

        const float minimumActiveHandHeight =
            result.hmd->world.translate.z - a_settings.handsBelowHeadLimit;
        for (std::size_t index = 0; index < a_input.hands.size(); ++index) {
            const auto& hand = a_input.hands[index];
            const bool heldByHiggs = a_physical.IsHeldByHand(index == 0);
            if (heldByHiggs && hand.valid && hand.position.z < minimumActiveHandHeight) {
                logger::info(
                    "Flight hand-height limit crossed hand={} handZ={:.1f} hmdZ={:.1f} minimumZ={:.1f}",
                    index == 0 ? "left" : "right",
                    hand.position.z,
                    result.hmd->world.translate.z,
                    minimumActiveHandHeight);
                result.status = FlightSafetyStatus::kStop;
                result.stopReason = "hand lowered below active height limit";
                return result;
            }
        }
        return result;
    }

    bool FlightSafety::IsAirborne(const RE::PlayerCharacter& a_player)
    {
        auto* controller = a_player.GetCharController();
        return controller &&
            (controller->supportBody.get() == nullptr ||
                controller->context.currentState == RE::hkpCharacterStateType::kInAir);
    }
}

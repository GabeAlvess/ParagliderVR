#include "pch.h"
#include "Config.h"
#include "FlightSafety.h"
#include "ParagliderActivation.h"
#include "PhysicalParagliderController.h"

namespace ParagliderVR
{
    void ParagliderActivation::ResetCalibration()
    {
        _calibration.Reset();
    }

    void ParagliderActivation::BlockUntilGripRelease()
    {
        _blockedUntilGripRelease = true;
    }

    bool ParagliderActivation::IsBlocked(const ParagliderInputState& a_input)
    {
        if (!_blockedUntilGripRelease) {
            return false;
        }
        if (!a_input.hands[0].gripDown && !a_input.hands[1].gripDown) {
            _blockedUntilGripRelease = false;
            logger::info("Paraglider activation unblocked after both grips released");
        }
        return true;
    }

    bool ParagliderActivation::Update(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const ParagliderInputState& a_input,
        PhysicalParagliderController& a_physical)
    {
        auto* vrData = a_player.GetVRNodeData();
        auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
        const auto& settings = Config::GetSingleton().Get();
        const float minimumActivationHeight = hmd ?
            hmd->world.translate.z + settings.handsAboveHead : 0.0f;
        for (std::size_t index = 0; index < a_input.hands.size(); ++index) {
            const auto& hand = a_input.hands[index];
            if (!hand.gripDown) {
                _gripPressedInsideActivationArea[index] = false;
            } else if (hand.gripPressed) {
                _gripPressedInsideActivationArea[index] =
                    hmd && hand.valid && hand.position.z >= minimumActivationHeight;
                logger::info(
                    "{} activation grip qualified={} handZ={:.1f} minimumZ={:.1f}",
                    index == 0 ? "Left" : "Right",
                    _gripPressedInsideActivationArea[index],
                    hand.position.z,
                    minimumActivationHeight);
            }
        }
        const bool handsInsideActivationArea = hmd &&
            a_input.hands[0].valid && a_input.hands[1].valid &&
            a_input.hands[0].position.z >= minimumActivationHeight &&
            a_input.hands[1].position.z >= minimumActivationHeight;
        const bool activationGesture = handsInsideActivationArea &&
            (settings.calibrationMode || FlightSafety::IsAirborne(a_player)) &&
            a_input.hands[0].gripDown && a_input.hands[1].gripDown &&
            _gripPressedInsideActivationArea[0] &&
            _gripPressedInsideActivationArea[1];
        if (!a_physical.IsDeployed() && activationGesture) {
            a_physical.Deploy(a_player);
            if (settings.calibrationMode && a_physical.IsDeployed()) {
                _calibration.OnDeployed();
            }
        }
        if (settings.calibrationMode) {
            _calibration.Update(a_player, a_delta, a_input, a_physical);
            return false;
        }
        if (a_physical.IsDeployed() &&
            !a_input.hands[0].gripDown &&
            !a_input.hands[1].gripDown) {
            a_physical.Retract();
        } else if (a_physical.IsReadyForFlight()) {
            _gripPressedInsideActivationArea = {};
            return true;
        }
        return false;
    }
}

#include "pch.h"
#include "ParagliderCalibration.h"
#include "PhysicalParagliderController.h"

namespace ParagliderVR
{
    void ParagliderCalibration::OnDeployed()
    {
        Reset();
        _awaitingGripRelease = true;
        logger::info(
            "Calibration spawned with HIGGS blocked; waiting for both activation grips to release");
        RE::DebugNotification("Release both grips, then grab anywhere");
    }

    void ParagliderCalibration::Update(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const ParagliderInputState& a_input,
        PhysicalParagliderController& a_physical)
    {
        if (!a_physical.IsDeployed()) {
            Reset();
            return;
        }
        if (_awaitingGripRelease) {
            _holdSeconds.fill(0.0f);
            _loggedSeconds.fill(0);
            if (!a_input.hands[0].gripDown && !a_input.hands[1].gripDown) {
                a_physical.EnableCalibrationGrabbing();
                _awaitingGripRelease = false;
                logger::info("Calibration ready for unrestricted natural HIGGS grabs");
                RE::DebugNotification("Calibration ready: grab where you want");
            }
            return;
        }

        const std::array held{
            a_physical.IsHeldByHand(true),
            a_physical.IsHeldByHand(false)
        };
        for (std::size_t index = 0; index < held.size(); ++index) {
            if (held[index]) {
                _holdSeconds[index] =
                    (std::min)(_holdSeconds[index] + a_delta, kHoldSeconds);
                const int completedSeconds = static_cast<int>(_holdSeconds[index]);
                if (completedSeconds > _loggedSeconds[index] && completedSeconds < 5) {
                    _loggedSeconds[index] = completedSeconds;
                    logger::info(
                        "Calibration {} hand hold {}/5 seconds",
                        index == 0 ? "left" : "right",
                        completedSeconds);
                }
            } else {
                if (_holdSeconds[index] > 0.0f) {
                    logger::info(
                        "Calibration {} hand hold reset before 5 seconds",
                        index == 0 ? "left" : "right");
                }
                _holdSeconds[index] = 0.0f;
                _loggedSeconds[index] = 0;
            }
        }
        if (!held[0] && !held[1]) {
            _captured = false;
        }
        if (!_captured &&
            _holdSeconds[0] >= kHoldSeconds &&
            _holdSeconds[1] >= kHoldSeconds) {
            if (a_physical.CaptureCalibration(a_player)) {
                _captured = true;
                RE::DebugNotification("Paraglider calibration captured");
                logger::info(
                    "Calibration automatically captured after both hands held for 5 seconds");
            } else {
                RE::DebugNotification("Hold the paraglider with both hands");
                logger::warn(
                    "Timed calibration completed but both HIGGS hands were not available at capture");
            }
        }
    }

    void ParagliderCalibration::Reset()
    {
        _holdSeconds.fill(0.0f);
        _loggedSeconds.fill(0);
        _captured = false;
        _awaitingGripRelease = false;
    }
}

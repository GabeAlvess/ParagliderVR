#include "pch.h"
#include "GliderPose.h"
#include "ParagliderCalibration.h"
#include "PhysicalParagliderController.h"

namespace ParagliderVR
{
    namespace
    {
        constexpr std::array kGestureNames{
            "Forward"sv,
            "ReduceSpeed"sv,
            "TurnLeft"sv,
            "TurnRight"sv,
            "Climb"sv,
            "Descend"sv
        };

        constexpr std::array kGesturePrompts{
            "Forward"sv,
            "Reduce speed"sv,
            "Turn left"sv,
            "Turn right"sv,
            "Climb"sv,
            "Descend"sv
        };

        float RotationDifferenceRadians(
            const RE::NiMatrix3& a_left,
            const RE::NiMatrix3& a_right)
        {
            float matrixProductTrace = 0.0f;
            for (std::size_t row = 0; row < 3; ++row) {
                for (std::size_t column = 0; column < 3; ++column) {
                    matrixProductTrace +=
                        a_left.entry[row][column] * a_right.entry[row][column];
                }
            }
            return std::acos(std::clamp(
                (matrixProductTrace - 1.0f) * 0.5f,
                -1.0f,
                1.0f));
        }
    }

    void ParagliderCalibration::OnDeployed()
    {
        Reset();
        _awaitingGripRelease = true;
        logger::info(
            "Gesture calibration spawned with HIGGS blocked; waiting for both activation grips to release");
        RE::DebugNotification("Release both grips to start gesture calibration");
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
            ResetHold();
            if (!a_input.hands[0].gripDown && !a_input.hands[1].gripDown) {
                a_physical.EnableCalibrationGrabbing();
                _awaitingGripRelease = false;
                logger::info("Gesture calibration ready for unrestricted natural HIGGS grabs");
                const std::string prompt = std::format(
                    "Calibration 1/{}: {} - hold both grips for 5 seconds",
                    kGestureNames.size(),
                    kGesturePrompts[0]);
                RE::DebugNotification(prompt.c_str());
            }
            return;
        }

        if (_stageIndex >= kGestureNames.size()) {
            return;
        }

        const bool leftHeld = a_physical.IsHeldByHand(true);
        const bool rightHeld = a_physical.IsHeldByHand(false);
        const bool bothHeld = leftHeld && rightHeld;
        GesturePose pose{};
        const bool poseAvailable = ResolveGesturePose(a_player, a_input, pose);
        if (!_hasLoggedInputState || leftHeld != _lastLeftHeld ||
            rightHeld != _lastRightHeld ||
            a_input.hands[0].gripDown != _lastLeftGripDown ||
            a_input.hands[1].gripDown != _lastRightGripDown ||
            poseAvailable != _lastPoseAvailable) {
            logger::info(
                "GESTURE CALIBRATION STATE heldLeft={} heldRight={} gripLeft={} gripRight={} poseAvailable={}",
                leftHeld,
                rightHeld,
                a_input.hands[0].gripDown,
                a_input.hands[1].gripDown,
                poseAvailable);
            _hasLoggedInputState = true;
            _lastLeftHeld = leftHeld;
            _lastRightHeld = rightHeld;
            _lastLeftGripDown = a_input.hands[0].gripDown;
            _lastRightGripDown = a_input.hands[1].gripDown;
            _lastPoseAvailable = poseAvailable;
        }
        if (_waitingForPoseChange) {
            if (bothHeld && poseAvailable && HasPoseChanged(pose)) {
                _waitingForPoseChange = false;
                ResetHold();
                logger::info(
                    "GESTURE CALIBRATION READY control={} step={}/{} pose change detected",
                    kGestureNames[_stageIndex],
                    _stageIndex + 1,
                    kGestureNames.size());
                const std::string prompt = std::format(
                    "Calibration {}/{}: {} - hold for 5 seconds",
                    _stageIndex + 1,
                    kGestureNames.size(),
                    kGesturePrompts[_stageIndex]);
                RE::DebugNotification(prompt.c_str());
            }
            return;
        }

        if (!bothHeld || !poseAvailable) {
            if (_stageStarted) {
                logger::info(
                    "GESTURE CALIBRATION INTERRUPTED control={} step={}/{} hold={:.2f}",
                    kGestureNames[_stageIndex],
                    _stageIndex + 1,
                    kGestureNames.size(),
                    _holdSeconds);
            }
            ResetHold();
            return;
        }

        if (!_stageStarted) {
            BeginCurrentStage();
        }
        _holdSeconds = (std::min)(_holdSeconds + a_delta, kHoldSeconds);
        const int completedSeconds = static_cast<int>(_holdSeconds);
        if (completedSeconds > _loggedSeconds && completedSeconds < 5) {
            _loggedSeconds = completedSeconds;
            logger::info(
                "GESTURE CALIBRATION HOLD control={} step={}/{} seconds={}/5",
                kGestureNames[_stageIndex],
                _stageIndex + 1,
                kGestureNames.size(),
                completedSeconds);
        }
        if (_holdSeconds >= kHoldSeconds) {
            CaptureCurrentStage(a_player, a_input, a_physical);
        }
    }

    void ParagliderCalibration::Reset()
    {
        ResetHold();
        _lastCapturedPose = {};
        _stageIndex = 0;
        _waitingForPoseChange = false;
        _hasCapturedPose = false;
        _awaitingGripRelease = false;
        _hasLoggedInputState = false;
    }

    bool ParagliderCalibration::ResolveGesturePose(
        RE::PlayerCharacter& a_player,
        const ParagliderInputState& a_input,
        GesturePose& a_pose) const
    {
        auto* vrData = a_player.GetVRNodeData();
        auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
        if (!hmd || !a_input.hands[0].valid || !a_input.hands[1].valid) {
            return false;
        }
        const auto inverseHmd = hmd->world.Invert();
        for (std::size_t index = 0; index < a_pose.handsInHmd.size(); ++index) {
            a_pose.handsInHmd[index] = inverseHmd * a_input.hands[index].worldTransform;
        }
        return true;
    }

    bool ParagliderCalibration::HasPoseChanged(const GesturePose& a_pose) const
    {
        if (!_hasCapturedPose) {
            return true;
        }
        for (std::size_t index = 0; index < a_pose.handsInHmd.size(); ++index) {
            const auto& previous = _lastCapturedPose.handsInHmd[index];
            const auto& current = a_pose.handsInHmd[index];
            if ((current.translate - previous.translate).Length() >= kPoseChangeDistance ||
                RotationDifferenceRadians(previous.rotate, current.rotate) >=
                    kPoseChangeAngleRadians) {
                return true;
            }
        }
        return false;
    }

    void ParagliderCalibration::BeginCurrentStage()
    {
        _stageStarted = true;
        _holdSeconds = 0.0f;
        _loggedSeconds = 0;
        logger::info(
            "GESTURE CALIBRATION BEGIN control={} step={}/{} holdSeconds={:.1f}",
            kGestureNames[_stageIndex],
            _stageIndex + 1,
            kGestureNames.size(),
            kHoldSeconds);
    }

    void ParagliderCalibration::CaptureCurrentStage(
        RE::PlayerCharacter& a_player,
        const ParagliderInputState& a_input,
        PhysicalParagliderController& a_physical)
    {
        GesturePose pose{};
        if (!ResolveGesturePose(a_player, a_input, pose) ||
            !a_physical.CaptureCalibration(a_player)) {
            logger::warn(
                "GESTURE CALIBRATION CAPTURE FAILED control={} step={}/{}",
                kGestureNames[_stageIndex],
                _stageIndex + 1,
                kGestureNames.size());
            RE::DebugNotification("Hold the paraglider with both hands");
            ResetHold();
            return;
        }

        const std::string leftLabel = std::format(
            "Gesture{}LeftHandInHmd",
            kGestureNames[_stageIndex]);
        const std::string rightLabel = std::format(
            "Gesture{}RightHandInHmd",
            kGestureNames[_stageIndex]);
        LogCalibrationTransform(leftLabel, pose.handsInHmd[0]);
        LogCalibrationTransform(rightLabel, pose.handsInHmd[1]);
        LogCalibrationTransform(
            std::format("Gesture{}LeftToRightHand", kGestureNames[_stageIndex]),
            a_input.hands[0].worldTransform.Invert() *
                a_input.hands[1].worldTransform);
        logger::info(
            "GESTURE CALIBRATION END control={} step={}/{}",
            kGestureNames[_stageIndex],
            _stageIndex + 1,
            kGestureNames.size());

        _lastCapturedPose = pose;
        _hasCapturedPose = true;
        ++_stageIndex;
        ResetHold();
        if (_stageIndex >= kGestureNames.size()) {
            logger::info("GESTURE CALIBRATION COMPLETE stages={}", kGestureNames.size());
            RE::DebugNotification("Gesture calibration complete");
            return;
        }

        _waitingForPoseChange = true;
        const std::string prompt = std::format(
            "Captured. Move to {}/{}: {}",
            _stageIndex + 1,
            kGestureNames.size(),
            kGesturePrompts[_stageIndex]);
        RE::DebugNotification(prompt.c_str());
    }

    void ParagliderCalibration::ResetHold()
    {
        _holdSeconds = 0.0f;
        _loggedSeconds = 0;
        _stageStarted = false;
    }
}

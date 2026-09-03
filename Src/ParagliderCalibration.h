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
        static constexpr float kPoseChangeDistance = 5.0f;
        static constexpr float kPoseChangeAngleRadians = 0.13962634f;

        struct GesturePose
        {
            std::array<RE::NiTransform, 2> handsInHmd{};
        };

        [[nodiscard]] bool ResolveGesturePose(
            RE::PlayerCharacter& a_player,
            const ParagliderInputState& a_input,
            GesturePose& a_pose) const;
        [[nodiscard]] bool HasPoseChanged(const GesturePose& a_pose) const;
        void BeginCurrentStage();
        void CaptureCurrentStage(
            RE::PlayerCharacter& a_player,
            const ParagliderInputState& a_input,
            PhysicalParagliderController& a_physical);
        void ResetHold();

        GesturePose _lastCapturedPose{};
        float _holdSeconds = 0.0f;
        int _loggedSeconds = 0;
        std::size_t _stageIndex = 0;
        bool _stageStarted = false;
        bool _waitingForPoseChange = false;
        bool _hasCapturedPose = false;
        bool _awaitingGripRelease = false;
        bool _hasLoggedInputState = false;
        bool _lastLeftHeld = false;
        bool _lastRightHeld = false;
        bool _lastLeftGripDown = false;
        bool _lastRightGripDown = false;
        bool _lastPoseAvailable = false;
    };
}

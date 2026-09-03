#pragma once

namespace HiggsPluginAPI
{
    struct IHiggsInterface001;
}

namespace ParagliderVR
{
    class HiggsGliderGrabController final
    {
    public:
        void Initialize();
        void BlockHandsForSpawn();
        void RestoreHandsAfterSpawn();
        void SetReference(RE::TESObjectREFR* a_reference);
        void Cancel();
        void ForceReleaseReference();
        [[nodiscard]] bool UpdateForcedRelease();
        void UpdateAlignment(float a_delta, bool a_leftGripDown, bool a_rightGripDown);
        void EnableCalibrationGrabbing();

        [[nodiscard]] bool IsAvailable() const;
        [[nodiscard]] bool IsHeldByHand(bool a_isLeft) const;
        [[nodiscard]] bool IsHeldByBothHands() const;
        [[nodiscard]] bool IsHeldByEitherHand() const;
        [[nodiscard]] bool IsReadyForFlight() const;
        [[nodiscard]] bool IsAnchorReleased() const;
        bool ReleaseCalibrationAnchorIfHeld();
        [[nodiscard]] bool CanCaptureCalibration() const;
        void LogGrabTransforms() const;

    private:
        enum class GrabPhase
        {
            kNone,
            kSettling,
            kReady,
            kBothRequested,
            kAligning,
            kComplete
        };

        static void OnHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference);
        static void OnHiggsDropped(bool a_isLeft, RE::TESObjectREFR* a_reference);
        void HandleHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference);
        void HandleHiggsDropped(bool a_isLeft, RE::TESObjectREFR* a_reference);
        [[nodiscard]] bool ApplyAuthoredGrabTransform(bool a_isLeft) const;
        void RequestInitialTwoHandGrab();

        inline static HiggsGliderGrabController* activeInstance_ = nullptr;
        HiggsPluginAPI::IHiggsInterface001* _higgsInterface = nullptr;
        RE::TESObjectREFR* _reference = nullptr;
        GrabPhase _grabPhase = GrabPhase::kNone;
        bool _initialGrabComplete = false;
        bool _higgsHandsBlockedForSpawn = false;
        bool _restoreLeftHiggsHand = false;
        bool _restoreRightHiggsHand = false;
        bool _higgsGrabbedCallbackRegistered = false;
        bool _higgsDroppedCallbackRegistered = false;
        bool _forcedReleasePending = false;
        bool _restoreLeftAfterForcedRelease = false;
        bool _restoreRightAfterForcedRelease = false;
        std::atomic_bool _calibrationLeftHeld = false;
        std::atomic_bool _calibrationRightHeld = false;
        bool _wasHeldByBothHands = false;
        float _grabRetryTimer = 0.0f;
    };
}

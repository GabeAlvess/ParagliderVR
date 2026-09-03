#pragma once

namespace ParagliderVR
{
    class PhysicalParagliderController final
    {
    public:
        static PhysicalParagliderController& GetSingleton();

        void InitializeHiggs();
        void SetEnabled(bool a_enabled);
        void Update(RE::PlayerCharacter& a_player, float a_delta);
        void Deploy(RE::PlayerCharacter& a_player);
        void Retract();
        void Reset();
        void EnableCalibrationGrabbing();

        [[nodiscard]] bool IsEquipped() const;
        [[nodiscard]] bool IsDeployed() const;
        [[nodiscard]] bool IsHeldByHand(bool a_isLeft) const;
        [[nodiscard]] bool IsHeldByBothHands() const;
        [[nodiscard]] bool IsHeldByEitherHand() const;
        [[nodiscard]] bool IsReadyForFlight() const;
        [[nodiscard]] bool CaptureCalibration(RE::PlayerCharacter& a_player) const;

    private:
        using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

        enum class GrabPhase
        {
            kNone,
            kSettling,
            kReady,
            kBothRequested,
            kAligning,
            kComplete
        };

        void ResolveForms();
        void PreloadPhysicalModel();
        void RefreshAvailableSlot(RE::PlayerCharacter& a_player);
        void SpawnPhysicalReference(RE::PlayerCharacter& a_player);
        void RequestInitialTwoHandGrab();
        void DestroyPhysicalReference();
        [[nodiscard]] std::string GetGrabbedNodeName(bool a_isLeft) const;
        [[nodiscard]] bool ResolveAuthoredGrabTransform(bool a_isLeft, RE::NiTransform& a_transform) const;
        [[nodiscard]] bool ApplyAuthoredGrabTransform(bool a_isLeft) const;
        void DisableAutomaticGrabNodes() const;
        void BlockHiggsHandsForSpawn();
        void RestoreHiggsHandsAfterSpawn();
        static void OnHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference);
        void HandleHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference);
        [[nodiscard]] static BipedSlot SlotMask(std::uint32_t a_slotNumber);

        RE::TESObjectARMO* _armor = nullptr;
        RE::TESObjectARMA* _armorAddon = nullptr;
        RE::TESObjectACTI* _physicalBase = nullptr;
        RE::NiPointer<RE::NiNode> _preloadedPhysicalModel;
        RE::NiPointer<RE::TESObjectREFR> _physicalReference;
        BipedSlot _assignedSlot = BipedSlot::kCirclet;
        GrabPhase _grabPhase = GrabPhase::kNone;
        bool _enabled = false;
        bool _initialGrabComplete = false;
        bool _higgsHandsBlockedForSpawn = false;
        bool _restoreLeftHiggsHand = false;
        bool _restoreRightHiggsHand = false;
        bool _higgsGrabbedCallbackRegistered = false;
        bool _physicalModelPreloadAttempted = false;
        float _slotRefreshTimer = 0.0f;
        float _grabRetryTimer = 0.0f;
    };
}

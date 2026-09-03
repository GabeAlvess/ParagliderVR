#pragma once

#include "HiggsGliderGrabController.h"
#include "PhysicalGliderEquipment.h"

namespace ParagliderVR
{
    struct ParagliderInputState;

    class PhysicalParagliderController final
    {
    public:
        static PhysicalParagliderController& GetSingleton();

        void InitializeHiggs();
        void SetEnabled(bool a_enabled);
        void Update(
            RE::PlayerCharacter& a_player,
            float a_delta,
            const ParagliderInputState& a_input);
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
        void SpawnPhysicalReference(RE::PlayerCharacter& a_player);
        void DestroyPhysicalReference();
        void RequestPhysicalReferenceDestroy();
        void DisableAutomaticGrabNodes() const;
        PhysicalGliderEquipment _equipment;
        HiggsGliderGrabController _higgsGrab;
        RE::NiPointer<RE::TESObjectREFR> _physicalReference;
        bool _destroyPending = false;
        bool _enabled = false;
    };
}

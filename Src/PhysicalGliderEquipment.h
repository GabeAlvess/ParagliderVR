#pragma once

namespace ParagliderVR
{
    class PhysicalGliderEquipment final
    {
    public:
        void Refresh(RE::PlayerCharacter& a_player, float a_delta);
        [[nodiscard]] bool IsAvailable() const noexcept;
        [[nodiscard]] bool IsEquipped() const;
        [[nodiscard]] RE::TESObjectACTI* GetPhysicalBase() const noexcept;

    private:
        using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

        void ResolveForms();
        void PreloadPhysicalModel();
        void RefreshAvailableSlot(RE::PlayerCharacter& a_player);
        [[nodiscard]] static BipedSlot SlotMask(std::uint32_t a_slotNumber);

        RE::TESObjectARMO* _armor = nullptr;
        RE::TESObjectARMA* _armorAddon = nullptr;
        RE::TESObjectACTI* _physicalBase = nullptr;
        RE::NiPointer<RE::NiNode> _preloadedPhysicalModel;
        BipedSlot _assignedSlot = BipedSlot::kCirclet;
        bool _physicalModelPreloadAttempted = false;
        float _slotRefreshTimer = 0.0f;
    };
}

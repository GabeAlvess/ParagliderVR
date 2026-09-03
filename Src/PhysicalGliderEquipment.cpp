#include "pch.h"
#include "PhysicalGliderEquipment.h"

#include <RE/B/BSModelDB.h>

namespace ParagliderVR
{
    namespace
    {
        constexpr std::string_view kPluginName = "ParagliderVR.esp";
        constexpr RE::FormID kArmorLocalFormID = 0x000800;
        constexpr RE::FormID kArmorAddonLocalFormID = 0x000801;
        constexpr RE::FormID kPhysicalCarrierLocalFormID = 0x000802;
        constexpr std::string_view kPhysicalModelPath = "Paraglider\\GliderPhysical.nif";
        constexpr std::uint32_t kPreferredSlot = 42;
        constexpr std::uint32_t kMinimumSlot = 30;
        constexpr std::uint32_t kMaximumSlot = 61;
        constexpr float kSlotRefreshSeconds = 0.25f;
    }

    void PhysicalGliderEquipment::Refresh(RE::PlayerCharacter& a_player, float a_delta)
    {
        ResolveForms();
        if (!IsAvailable()) {
            return;
        }
        if (IsEquipped()) {
            PreloadPhysicalModel();
            return;
        }
        _slotRefreshTimer += a_delta;
        if (_slotRefreshTimer >= kSlotRefreshSeconds) {
            _slotRefreshTimer = 0.0f;
            RefreshAvailableSlot(a_player);
        }
    }

    bool PhysicalGliderEquipment::IsAvailable() const noexcept
    {
        return _armor && _armorAddon && _physicalBase;
    }

    bool PhysicalGliderEquipment::IsEquipped() const
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return player && _armor && player->GetWornArmor(_armor->GetSlotMask()) == _armor;
    }

    RE::TESObjectACTI* PhysicalGliderEquipment::GetPhysicalBase() const noexcept
    {
        return _physicalBase;
    }

    void PhysicalGliderEquipment::ResolveForms()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return;
        }
        if (!_armor) {
            _armor = dataHandler->LookupForm<RE::TESObjectARMO>(kArmorLocalFormID, kPluginName);
        }
        if (!_armorAddon) {
            _armorAddon = dataHandler->LookupForm<RE::TESObjectARMA>(kArmorAddonLocalFormID, kPluginName);
        }
        if (!_physicalBase) {
            _physicalBase =
                dataHandler->LookupForm<RE::TESObjectACTI>(kPhysicalCarrierLocalFormID, kPluginName);
        }
    }

    void PhysicalGliderEquipment::PreloadPhysicalModel()
    {
        if (_physicalModelPreloadAttempted) {
            return;
        }
        _physicalModelPreloadAttempted = true;
        logger::info("Physical paraglider model preload begin path='{}'", kPhysicalModelPath);
        const auto started = std::chrono::steady_clock::now();
        RE::BSModelDB::DBTraits::ArgsType arguments{};
        const auto result = RE::BSModelDB::Demand(
            kPhysicalModelPath.data(),
            _preloadedPhysicalModel,
            arguments);
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        if (result != RE::BSResource::ErrorCode::kNone || !_preloadedPhysicalModel) {
            logger::warn(
                "Physical paraglider model preload failed error={} elapsedMs={:.1f}",
                static_cast<int>(result),
                elapsed);
            return;
        }
        logger::info("Physical paraglider model preload complete elapsedMs={:.1f}", elapsed);
    }

    void PhysicalGliderEquipment::RefreshAvailableSlot(RE::PlayerCharacter& a_player)
    {
        if (!_armor || !_armorAddon || IsEquipped()) {
            return;
        }
        BipedSlot selected = SlotMask(kPreferredSlot);
        bool found = false;
        for (std::uint32_t distance = 0;
             distance <= kMaximumSlot - kMinimumSlot && !found;
             ++distance) {
            const std::array candidates{
                static_cast<std::int32_t>(kPreferredSlot) - static_cast<std::int32_t>(distance),
                static_cast<std::int32_t>(kPreferredSlot) + static_cast<std::int32_t>(distance)
            };
            for (std::size_t index = 0; index < candidates.size(); ++index) {
                if (distance == 0 && index == 1) {
                    continue;
                }
                const auto slotNumber = candidates[index];
                if (slotNumber < static_cast<std::int32_t>(kMinimumSlot) ||
                    slotNumber > static_cast<std::int32_t>(kMaximumSlot)) {
                    continue;
                }
                const auto mask = SlotMask(static_cast<std::uint32_t>(slotNumber));
                if (!a_player.GetWornArmor(mask)) {
                    selected = mask;
                    found = true;
                    break;
                }
            }
        }
        if (selected != _assignedSlot) {
            _assignedSlot = selected;
            _armor->SetSlotMask(selected);
            _armorAddon->SetSlotMask(selected);
            logger::info(
                "Paraglider biped slot selected={} mask=0x{:08X}",
                std::countr_zero(static_cast<std::uint32_t>(selected)) + kMinimumSlot,
                static_cast<std::uint32_t>(selected));
        }
    }

    PhysicalGliderEquipment::BipedSlot PhysicalGliderEquipment::SlotMask(
        std::uint32_t a_slotNumber)
    {
        return static_cast<BipedSlot>(1u << (a_slotNumber - kMinimumSlot));
    }
}

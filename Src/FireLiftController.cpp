#include "pch.h"
#include "Config.h"
#include "FireLiftController.h"
#include "ParagliderBallisticController.h"

namespace ParagliderVR
{
    namespace
    {
        std::string Lower(std::string_view a_value)
        {
            std::string result(a_value);
            std::ranges::transform(result, result.begin(), [](unsigned char a_character) {
                return static_cast<char>(std::tolower(a_character));
            });
            return result;
        }

        bool ContainsFireSignature(std::string_view a_value)
        {
            if (a_value.empty()) {
                return false;
            }
            const auto value = Lower(a_value);
            if (value.contains("firewood") ||
                value.contains("firebolt") ||
                value.contains("fireball") ||
                value.contains("firestorm")) {
                return false;
            }
            constexpr std::array signatures{
                "campfire"sv,
                "bonfire"sv,
                "brazier"sv,
                "firepit"sv,
                "hearth"sv,
                "forge"sv,
                "embers"sv,
                "burning"sv,
                "flame"sv,
                "fire"sv
            };
            return std::ranges::any_of(signatures, [&](std::string_view a_signature) {
                return value.contains(a_signature);
            });
        }

        const char* GetModelPath(const RE::TESBoundObject& a_base)
        {
            if (const auto* activator = a_base.As<RE::TESObjectACTI>()) {
                return activator->GetModel();
            }
            if (const auto* light = a_base.As<RE::TESObjectLIGH>()) {
                return light->GetModel();
            }
            if (const auto* object = a_base.As<RE::TESObjectSTAT>()) {
                return object->GetModel();
            }
            if (const auto* object = a_base.As<RE::BGSMovableStatic>()) {
                return object->GetModel();
            }
            if (const auto* object = a_base.As<RE::BGSStaticCollection>()) {
                return object->GetModel();
            }
            return nullptr;
        }

        bool IsFireSource(const RE::TESObjectREFR& a_reference)
        {
            if (a_reference.IsDisabled()) {
                return false;
            }
            const auto* base = a_reference.GetBaseObject();
            if (!base || base->IsDeleted()) {
                return false;
            }
            if (const auto* light = base->As<RE::TESObjectLIGH>(); light && light->CanBeCarried()) {
                return false;
            }
            const char* referenceEditorID = a_reference.GetFormEditorID();
            const char* baseEditorID = base->GetFormEditorID();
            const char* name = a_reference.GetName();
            const char* modelPath = GetModelPath(*base);
            return ContainsFireSignature(referenceEditorID ? referenceEditorID : "") ||
                ContainsFireSignature(baseEditorID ? baseEditorID : "") ||
                ContainsFireSignature(name ? name : "") ||
                ContainsFireSignature(modelPath ? modelPath : "");
        }
    }

    void FireLiftController::Reset()
    {
        _scanTimer = 0.0f;
        _insideFireColumn = false;
    }

    void FireLiftController::Update(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const Settings& a_settings)
    {
        if (!a_settings.fireLiftEnabled) {
            Reset();
            return;
        }
        _scanTimer += a_delta;
        if (_scanTimer < a_settings.fireLiftScanInterval) {
            return;
        }
        _scanTimer = 0.0f;

        auto* world = RE::TES::GetSingleton();
        if (!world) {
            _insideFireColumn = false;
            return;
        }
        const auto playerPosition = a_player.GetPosition();
        RE::TESObjectREFR* detectedSource = nullptr;
        float closestHorizontalDistanceSquared =
            a_settings.fireLiftHorizontalRadius * a_settings.fireLiftHorizontalRadius;
        const float searchRadius = std::sqrt(
            (a_settings.fireLiftMaximumHeight * a_settings.fireLiftMaximumHeight) +
            closestHorizontalDistanceSquared);
        world->ForEachReferenceInRange(
            std::addressof(a_player),
            searchRadius,
            [&](RE::TESObjectREFR* a_reference) {
                if (!a_reference || a_reference == std::addressof(a_player) || !IsFireSource(*a_reference)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                const auto sourcePosition = a_reference->GetPosition();
                const float verticalDistance = playerPosition.z - sourcePosition.z;
                if (verticalDistance < 0.0f || verticalDistance > a_settings.fireLiftMaximumHeight) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                const float x = playerPosition.x - sourcePosition.x;
                const float y = playerPosition.y - sourcePosition.y;
                const float horizontalDistanceSquared = (x * x) + (y * y);
                if (horizontalDistanceSquared > closestHorizontalDistanceSquared) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                closestHorizontalDistanceSquared = horizontalDistanceSquared;
                detectedSource = a_reference;
                return RE::BSContainer::ForEachResult::kContinue;
            });

        const bool insideFireColumn = detectedSource != nullptr;
        if (insideFireColumn && !_insideFireColumn) {
            ParagliderBallisticController::GetSingleton().QueueVerticalImpulse(
                a_settings.fireLiftVerticalImpulse,
                a_settings.fireLiftImpulseDuration);
            logger::info(
                "Fire lift triggered source={:08X} name='{}' horizontalDistance={:.1f} verticalDistance={:.1f} impulse={:.1f} duration={:.2f}",
                detectedSource->formID,
                detectedSource->GetName() ? detectedSource->GetName() : "",
                std::sqrt(closestHorizontalDistanceSquared),
                playerPosition.z - detectedSource->GetPosition().z,
                a_settings.fireLiftVerticalImpulse,
                a_settings.fireLiftImpulseDuration);
        }
        _insideFireColumn = insideFireColumn;
    }
}

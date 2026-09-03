#include "pch.h"
#include "FlightStamina.h"
#include "Config.h"

namespace ParagliderVR
{
    void FlightStamina::Update(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const Settings& a_settings)
    {
        if (a_settings.staminaPerSecond <= 0.0f || _exhausted) {
            return;
        }

        auto* actorValueOwner = a_player.AsActorValueOwner();
        const float stamina = actorValueOwner ?
            actorValueOwner->GetActorValue(RE::ActorValue::kStamina) : 0.0f;
        const float cost = a_settings.staminaPerSecond * a_delta;
        if (RE::PlayerCharacter::IsGodMode()) {
            if (!_bypassLogged) {
                _bypassLogged = true;
                logger::info("Stamina consumption bypassed by player god mode");
            }
            return;
        }
        if (!actorValueOwner || !std::isfinite(stamina) || stamina <= cost) {
            _exhausted = true;
            RE::DebugNotification("Stamina depleted - paraglider performance reduced");
            logger::info(
                "Paraglider stamina exhausted stamina={:.2f} horizontalScale={:.2f} fallMultiplier={:.2f}",
                stamina,
                a_settings.exhaustedHorizontalSpeedScale,
                a_settings.exhaustedFallMultiplier);
            return;
        }

        const float before = stamina;
        actorValueOwner->RestoreActorValue(
            RE::ACTOR_VALUE_MODIFIER::kDamage,
            RE::ActorValue::kStamina,
            -cost);
        const float after = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);
        if (std::isfinite(after) && after >= before - 0.0001f) {
            if (!_bypassLogged) {
                _bypassLogged = true;
                logger::info(
                    "Stamina consumption bypassed because the game kept the value locked before={:.2f} after={:.2f}",
                    before,
                    after);
            }
        } else {
            _bypassLogged = false;
        }
    }

    void FlightStamina::Reset()
    {
        _bypassLogged = false;
        _exhausted = false;
    }

    bool FlightStamina::IsExhausted() const noexcept
    {
        return _exhausted;
    }
}

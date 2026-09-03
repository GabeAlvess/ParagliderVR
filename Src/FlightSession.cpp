#include "pch.h"
#include "AnimationOar.h"
#include "Config.h"
#include "FlightSession.h"
#include "ParagliderAudio.h"
#include "PhysicalParagliderController.h"
#include "VrikIntegration.h"

namespace ParagliderVR
{
    namespace
    {
        constexpr float kAirborneAnimationRefreshSeconds = 1.25f;

        bool RestartAirborneAnimation(RE::PlayerCharacter& a_player)
        {
            return a_player.NotifyAnimationGraph("JumpFall") ||
                a_player.NotifyAnimationGraph("JumpFallDirectional");
        }
    }

    void FlightSession::Start(
        RE::PlayerCharacter* a_player,
        float a_initialHorizontalSpeed)
    {
        _stamina.Reset();
        AnimationOar::SetActive(true);
        VrikIntegration::SetLowerBodyOverride(true);
        if (a_player) {
            ParagliderAudio::GetSingleton().StartFlightAudio(*a_player);
        }
        _animationRefreshTimer = 0.0f;
        const bool graphStarted = a_player && RestartAirborneAnimation(*a_player);
        RE::DebugNotification("Paraglider opened");
        logger::info(
            "Flight started airborneGraphStarted={} initialHorizontalSpeed={:.1f}",
            graphStarted,
            a_initialHorizontalSpeed);
    }

    void FlightSession::Stop(bool a_wasActive, std::string_view a_reason)
    {
        PhysicalParagliderController::GetSingleton().Retract();
        _windVisual.Hide();
        ParagliderAudio::GetSingleton().StopFlightAudio();
        if (!a_wasActive) {
            return;
        }

        AnimationOar::SetActive(false);
        VrikIntegration::SetLowerBodyOverride(false);
        _stamina.Reset();
        _animationRefreshTimer = 0.0f;
        logger::info("Flight stopped: {}", a_reason);
    }

    bool FlightSession::Update(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const Settings& a_settings)
    {
        ParagliderAudio::GetSingleton().UpdateFlightAudio(a_player, a_delta);
        _animationRefreshTimer += a_delta;
        if (_animationRefreshTimer >= kAirborneAnimationRefreshSeconds) {
            _animationRefreshTimer = 0.0f;
            logger::info("Airborne animation refreshed={}", RestartAirborneAnimation(a_player));
        }
        _stamina.Update(a_player, a_delta, a_settings);
        return _stamina.IsExhausted();
    }

    void FlightSession::UpdateAnimationDirection(float a_offhandThrottle)
    {
        AnimationOar::Direction direction = AnimationOar::Direction::kIdle;
        if (a_offhandThrottle > 0.10f) {
            direction = AnimationOar::Direction::kForward;
        } else if (a_offhandThrottle < -0.10f) {
            direction = AnimationOar::Direction::kBackward;
        }
        AnimationOar::SetDirection(direction);
    }

    void FlightSession::UpdateWind(
        RE::PlayerCharacter& a_player,
        const FlightCommand& a_command)
    {
        _windVisual.Update(a_player, a_command);
    }
}

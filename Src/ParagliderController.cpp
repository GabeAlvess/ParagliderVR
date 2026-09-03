#include "pch.h"
#include "ParagliderController.h"
#include "Config.h"
#include "ParagliderBallisticController.h"
#include "PhysicalParagliderController.h"

namespace ParagliderVR
{
    ParagliderController& ParagliderController::GetSingleton()
    {
        static ParagliderController singleton;
        return singleton;
    }

    void ParagliderController::SetEnabled(bool a_enabled)
    {
        _enabled = a_enabled;
        ParagliderInput::GetSingleton().SetEnabled(a_enabled);
        if (!a_enabled) {
            StopFlight("gameplay suspended");
            ParagliderBallisticController::GetSingleton().Abort();
            _activation.ResetCalibration();
        }
    }

    void ParagliderController::StartFlight()
    {
        if (!ParagliderBallisticController::GetSingleton().BeginGlideFlight()) {
            logger::warn("Flight activation rejected because ballistic control could not start");
            return;
        }
        const auto& settings = Config::GetSingleton().Get();
        _flightControl.Begin(RE::PlayerCharacter::GetSingleton(), settings);
        _fireLift.Reset();
        _flightSafety.Reset();
        {
            std::scoped_lock lock(_flightLock);
            _flight.active = true;
        }
        _flightSession.Start(
            RE::PlayerCharacter::GetSingleton(),
            _flightControl.GetCommandedHorizontalSpeed());
    }

    void ParagliderController::StopFlight(std::string_view a_reason)
    {
        bool wasActive = false;
        {
            std::scoped_lock lock(_flightLock);
            wasActive = _flight.active;
            _flight = {};
        }
        if (wasActive) {
            if (a_reason == "landed") {
                _activation.BlockUntilGripRelease();
            }
            _flightSafety.Reset();
            _gestureLogTimer = 0.0f;
            _flightControl.Reset();
            _fireLift.Reset();
        }
        _flightSession.Stop(wasActive, a_reason);
    }

    void ParagliderController::Update()
    {
        if (!_enabled) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->IsDead()) {
            StopFlight("player unavailable");
            ParagliderBallisticController::GetSingleton().Abort();
            return;
        }
        float delta = 0.011f;
        if (auto* timer = RE::BSTimer::GetSingleton(); timer && timer->delta > 0.0f) {
            delta = (std::min)(timer->delta, 0.05f);
        }
        auto& input = ParagliderInput::GetSingleton();
        input.RefreshPoses(*player);
        const auto inputState = input.GetState();
        auto& physical = PhysicalParagliderController::GetSingleton();
        physical.Update(*player, delta, inputState);
        if (!physical.IsEquipped()) {
            StopFlight("paraglider unequipped");
            return;
        }
        if (_activation.IsBlocked(inputState)) {
            return;
        }
        const bool active = [&]() {
            std::scoped_lock lock(_flightLock);
            return _flight.active;
        }();
        if (!active) {
            if (_activation.Update(*player, delta, inputState, physical)) {
                StartFlight();
            }
            return;
        }
        UpdateFlight(*player, delta, inputState);
    }

    void ParagliderController::UpdateFlight(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const ParagliderInputState& a_input)
    {
        auto& physical = PhysicalParagliderController::GetSingleton();
        const auto& settings = Config::GetSingleton().Get();
        const auto safety = _flightSafety.Evaluate(
            a_player,
            a_delta,
            a_input,
            physical,
            settings);
        if (safety.status == FlightSafetyStatus::kStop) {
            StopFlight(safety.stopReason);
            return;
        }
        if (safety.status == FlightSafetyStatus::kWaitForHands) {
            return;
        }
        _fireLift.Update(a_player, a_delta, settings);
        const bool staminaExhausted = _flightSession.Update(a_player, a_delta, settings);

        const auto control = _flightControl.BuildCommand(
            *safety.hmd,
            a_input,
            safety.heldHandCount == 2,
            staminaExhausted,
            a_delta,
            settings);
        _flightSession.UpdateAnimationDirection(control.offhandThrottle);

        const auto& state = control.command;
        {
            std::scoped_lock lock(_flightLock);
            _flight = state;
        }
        _flightSession.UpdateWind(a_player, state);
        _gestureLogTimer += a_delta;
        if (_gestureLogTimer >= 0.50f) {
            _gestureLogTimer = 0.0f;
            logger::info(
                "Control sample grips={} heldHands={} exhausted={} dominantY={:.2f} offhand=({:.2f},{:.2f}) gesture={} confidence={:.2f} gestureAxes=({:.2f},{:.2f},{:.2f}) verticalThrottle={:.2f} horizontalThrottle={:.2f} lateralThrottle={:.2f} commandedSpeed={:.1f} effectiveSpeed={:.1f} lateralSpeed={:.1f} verticalTarget={:.1f}",
                safety.gripCount,
                safety.heldHandCount,
                staminaExhausted,
                a_input.mainThumbstick.y,
                a_input.offThumbstick.x,
                a_input.offThumbstick.y,
                control.gestureIndex,
                control.gestureConfidence,
                control.gestureVerticalThrottle,
                control.gestureHorizontalThrottle,
                control.gestureLateralThrottle,
                control.dominantThrottle,
                control.offhandThrottle,
                control.lateralThrottle,
                control.commandedHorizontalSpeed,
                state.steeringSpeed,
                state.lateralSpeed,
                state.verticalTargetSpeed);
        }
    }

    bool ParagliderController::IsFlightActive() const
    {
        std::scoped_lock lock(_flightLock);
        return _flight.active;
    }

    FlightCommand ParagliderController::GetFlightCommand() const
    {
        std::scoped_lock lock(_flightLock);
        return _flight;
    }

}

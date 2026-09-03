#pragma once

#include <mutex>

namespace ParagliderVR
{
    class ParagliderController final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static ParagliderController& GetSingleton();
        void InstallInput();
        void SetEnabled(bool a_enabled);
        void Update();
        [[nodiscard]] bool IsFlightActive() const;
        [[nodiscard]] RE::NiPoint3 BuildVelocityDelta(const RE::NiPoint3& a_baseVelocity) const;

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_eventList,
            RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

    private:
        struct HandState
        {
            bool gripDown = false;
            RE::NiPoint3 position{};
            RE::NiPoint3 forward{};
            bool valid = false;
        };

        struct FlightState
        {
            bool active = false;
            bool dualHanded = false;
            float fallMultiplier = 1.0f;
            float referenceFallSpeed = 500.0f;
            float verticalTargetSpeed = 0.0f;
            float steeringSpeed = 0.0f;
            float horizontalAcceleration = 0.0f;
            float horizontalDeceleration = 0.0f;
            float verticalTransitionAcceleration = 0.0f;
            float deltaTime = 0.0f;
            RE::NiPoint3 steeringDirection{};
            RE::NiPoint3 lateralDirection{};
            float lateralSpeed = 0.0f;
        };

        struct ThumbstickState
        {
            float x = 0.0f;
            float y = 0.0f;
        };

        void RefreshHands(RE::PlayerCharacter& a_player);
        void StartFlight();
        void StopFlight(std::string_view a_reason);
        void UpdateFlight(RE::PlayerCharacter& a_player, float a_delta);
        void EnsureVisual(RE::PlayerCharacter& a_player);
        void UpdateVisual(RE::PlayerCharacter& a_player);
        void EnsureWindVisual(RE::PlayerCharacter& a_player);
        void UpdateWindVisual(RE::PlayerCharacter& a_player);
        void HideWindVisual();
        void HideVisual();
        [[nodiscard]] bool IsAirborne(const RE::PlayerCharacter& a_player) const;

        std::array<HandState, 2> _hands{};
        ThumbstickState _mainThumbstick{};
        ThumbstickState _offThumbstick{};
        mutable std::mutex _flightLock;
        FlightState _flight{};
        RE::NiPointer<RE::NiNode> _visual;
        RE::NiNode* _visualParent = nullptr;
        RE::NiPointer<RE::NiNode> _windVisual;
        RE::NiNode* _windVisualParent = nullptr;
        bool _enabled = false;
        bool _inputInstalled = false;
        bool _activationBlockedUntilGripRelease = false;
        std::array<float, 2> _calibrationHoldSeconds{};
        std::array<int, 2> _calibrationLoggedSeconds{};
        bool _calibrationCaptured = false;
        bool _calibrationAwaitingGripRelease = false;
        bool _visualTransformLogged = false;
        bool _windVisualTransformLogged = false;
        bool _staminaBypassLogged = false;
        bool _staminaExhausted = false;
        float _gestureLogTimer = 0.0f;
        float _animationRefreshTimer = 0.0f;
        float _commandedHorizontalSpeed = 0.0f;
        bool _singleHandVisualActive = false;
        std::size_t _singleHandVisualIndex = 0;
        RE::NiPoint3 _singleHandVisualOffset{};
        RE::NiMatrix3 _singleHandVisualRotation{};
    };
}

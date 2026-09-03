#pragma once

namespace ParagliderVR
{
    struct HandInputState
    {
        bool gripDown = false;
        bool gripPressed = false;
        RE::NiPoint3 position{};
        RE::NiPoint3 forward{};
        bool valid = false;
    };

    struct ThumbstickInputState
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct ParagliderInputState
    {
        std::array<HandInputState, 2> hands{};
        ThumbstickInputState mainThumbstick{};
        ThumbstickInputState offThumbstick{};
    };

    class ParagliderInput final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static ParagliderInput& GetSingleton();

        void Install();
        void SetEnabled(bool a_enabled);
        void RefreshPoses(RE::PlayerCharacter& a_player);
        [[nodiscard]] ParagliderInputState GetState();

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_eventList,
            RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

    private:
        ParagliderInputState _state{};
        bool _enabled = false;
        bool _installed = false;
    };
}

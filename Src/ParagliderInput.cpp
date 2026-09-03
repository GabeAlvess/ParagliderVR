#include "pch.h"
#include "ParagliderInput.h"

namespace ParagliderVR
{
    namespace
    {
        RE::NiPoint3 ControllerForward(const RE::NiAVObject& a_node)
        {
            const RE::NiPoint3 forward{
                -a_node.world.rotate.entry[0][2],
                -a_node.world.rotate.entry[1][2],
                -a_node.world.rotate.entry[2][2]
            };
            const float length = forward.Length();
            return length > 0.0001f ? forward / length : RE::NiPoint3{};
        }
    }

    ParagliderInput& ParagliderInput::GetSingleton()
    {
        static ParagliderInput singleton;
        return singleton;
    }

    void ParagliderInput::Install()
    {
        if (_installed) {
            return;
        }
        if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
            manager->AddEventSink(this);
            _installed = true;
            logger::info("VR paraglider input installed");
        } else {
            logger::critical("BSInputDeviceManager unavailable");
        }
    }

    void ParagliderInput::SetEnabled(bool a_enabled)
    {
        _enabled = a_enabled;
        if (!a_enabled) {
            _state = {};
        }
    }

    void ParagliderInput::RefreshPoses(RE::PlayerCharacter& a_player)
    {
        auto* vrData = a_player.GetVRNodeData();
        if (!vrData) {
            _state.hands[0].valid = false;
            _state.hands[1].valid = false;
            return;
        }
        std::array<RE::NiAVObject*, 2> nodes{
            vrData->LeftWandNode ? vrData->LeftWandNode.get() : vrData->NPCLHnd.get(),
            vrData->RightWandNode ? vrData->RightWandNode.get() : vrData->NPCRHnd.get()
        };
        for (std::size_t index = 0; index < _state.hands.size(); ++index) {
            auto& hand = _state.hands[index];
            auto* node = nodes[index];
            hand.valid = node != nullptr;
            if (node) {
                hand.worldTransform = node->world;
                hand.position = node->world.translate;
                hand.forward = ControllerForward(*node);
            }
        }
    }

    ParagliderInputState ParagliderInput::GetState()
    {
        const auto state = _state;
        for (auto& hand : _state.hands) {
            hand.gripPressed = false;
        }
        return state;
    }

    RE::BSEventNotifyControl ParagliderInput::ProcessEvent(
        RE::InputEvent* const* a_eventList,
        RE::BSTEventSource<RE::InputEvent*>*)
    {
        if (!_enabled || !a_eventList) {
            return RE::BSEventNotifyControl::kContinue;
        }
        for (auto* event = *a_eventList; event; event = event->next) {
            if (auto* thumbstick = event->AsThumbstickEvent()) {
                if (thumbstick->IsMainHand()) {
                    _state.mainThumbstick.x = std::clamp(thumbstick->xValue, -1.0f, 1.0f);
                    _state.mainThumbstick.y = std::clamp(thumbstick->yValue, -1.0f, 1.0f);
                } else if (thumbstick->IsOffHand()) {
                    _state.offThumbstick.x = std::clamp(thumbstick->xValue, -1.0f, 1.0f);
                    _state.offThumbstick.y = std::clamp(thumbstick->yValue, -1.0f, 1.0f);
                }
                continue;
            }
            auto* button = event->AsButtonEvent();
            if (!button ||
                button->GetIDCode() != static_cast<std::uint32_t>(vr::k_EButton_Grip)) {
                continue;
            }
            const auto device = button->GetDevice();
            const bool left = device == RE::INPUT_DEVICE::kOculusSecondary ||
                device == RE::INPUT_DEVICE::kViveSecondary ||
                device == RE::INPUT_DEVICE::kWMRSecondary;
            const bool right = device == RE::INPUT_DEVICE::kOculusPrimary ||
                device == RE::INPUT_DEVICE::kVivePrimary ||
                device == RE::INPUT_DEVICE::kWMRPrimary;
            if (!left && !right) {
                continue;
            }
            auto& hand = _state.hands[left ? 0 : 1];
            const bool pressed = button->IsPressed();
            if (hand.gripDown != pressed) {
                hand.gripDown = pressed;
                hand.gripPressed = pressed;
                logger::info(
                    "{} grip {}",
                    left ? "Left" : "Right",
                    pressed ? "pressed" : "released");
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
}

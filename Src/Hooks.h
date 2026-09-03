#pragma once

namespace ParagliderVR
{
    class Hooks final
    {
    public:
        static void Install();

    private:
        static void OnFrameUpdate();
        static void HookSetVelocity(RE::bhkCharacterController* a_controller, RE::hkVector4& a_velocity);
        static inline REL::Relocation<decltype(OnFrameUpdate)> _onFrameUpdate;
        static inline REL::Relocation<decltype(HookSetVelocity)> _setVelocity;
    };
}

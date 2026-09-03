#include "pch.h"
#include "Hooks.h"
#include "ParagliderBallisticController.h"
#include "ParagliderController.h"

namespace ParagliderVR
{
    void Hooks::Install()
    {
        auto& trampoline = SKSE::GetTrampoline();
        REL::Relocation<std::uintptr_t> mainUpdateBase{ REL::RelocationID(35565, 36564) };
        _onFrameUpdate = trampoline.write_call<5>(
            mainUpdateBase.address() + REL::Relocate(0x748, 0xc26, 0x7ee),
            OnFrameUpdate);
        REL::Relocation<std::uintptr_t> proxyVTable{ RE::VTABLE_bhkCharProxyController[1] };
        _setVelocity = proxyVTable.write_vfunc(0x07, HookSetVelocity);
        logger::info("Hooks installed mainUpdateBase=0x{:X}", mainUpdateBase.address());
    }

    void Hooks::OnFrameUpdate()
    {
        _onFrameUpdate();
        auto& paraglider = ParagliderController::GetSingleton();
        paraglider.Update();
        float delta = 0.011f;
        if (auto* timer = RE::BSTimer::GetSingleton(); timer && timer->delta > 0.0f) {
            delta = (std::min)(timer->delta, 0.05f);
        }
        ParagliderBallisticController::GetSingleton().Update(delta, paraglider.GetFlightCommand());
    }

    void Hooks::HookSetVelocity(RE::bhkCharacterController* a_controller, RE::hkVector4& a_velocity)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->GetCharController() != a_controller) {
            _setVelocity(a_controller, a_velocity);
            return;
        }
        const float worldScale = RE::bhkWorld::GetWorldScale();
        if (!std::isfinite(worldScale) || worldScale <= 0.000001f) {
            _setVelocity(a_controller, a_velocity);
            return;
        }
        const RE::NiPoint3 baseVelocity{
            a_velocity.quad.m128_f32[0] / worldScale,
            a_velocity.quad.m128_f32[1] / worldScale,
            a_velocity.quad.m128_f32[2] / worldScale
        };
        auto& ballistic = ParagliderBallisticController::GetSingleton();
        if (ballistic.IsApplyingVelocityUpdate()) {
            _setVelocity(a_controller, a_velocity);
            return;
        }
        const auto command = ParagliderController::GetSingleton().GetFlightCommand();
        const auto delta = CalculateVelocityDelta(command, baseVelocity);
        RE::hkVector4 finalVelocity(
            a_velocity.quad.m128_f32[0] + (delta.x * worldScale),
            a_velocity.quad.m128_f32[1] + (delta.y * worldScale),
            a_velocity.quad.m128_f32[2] + (delta.z * worldScale),
            0.0f);
        _setVelocity(a_controller, finalVelocity);
    }
}

#pragma once

namespace ParagliderVR
{
    struct GliderSpawnFrame
    {
        RE::NiTransform world{};
        RE::NiPoint3 localX{};
        RE::NiPoint3 forward{};
        RE::NiPoint3 up{};
        RE::NiPoint3 leftHand{};
        RE::NiPoint3 rightHand{};
        RE::NiPoint3 hmdForward{};
    };

    [[nodiscard]] RE::NiTransform CalibratedHandToGlider(bool a_isLeft);
    [[nodiscard]] bool ResolveGliderSpawnFrame(
        RE::PlayerCharacter& a_player,
        GliderSpawnFrame& a_frame);
    [[nodiscard]] bool ApplyGliderSpawnFrame(
        RE::TESObjectREFR& a_reference,
        const GliderSpawnFrame& a_frame);
    [[nodiscard]] float GetHmdForwardAlignment(const GliderSpawnFrame& a_frame);
    void LogCalibrationTransform(
        std::string_view a_label,
        const RE::NiTransform& a_transform);
}

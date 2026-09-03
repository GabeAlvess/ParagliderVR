#include "pch.h"
#include "GliderPose.h"

namespace ParagliderVR
{
    namespace
    {
        RE::NiTransform CalibratedHmdToGlider()
        {
            RE::NiTransform transform{};
            transform.translate = { 15.930664f, 9.117188f, 1.521729f };
            transform.scale = 1.0f;
            transform.rotate.entry[0][0] = 0.984169722f;
            transform.rotate.entry[0][1] = 0.176323295f;
            transform.rotate.entry[0][2] = -0.017881911f;
            transform.rotate.entry[1][0] = -0.176799282f;
            transform.rotate.entry[1][1] = 0.983791769f;
            transform.rotate.entry[1][2] = -0.029926598f;
            transform.rotate.entry[2][0] = 0.012315316f;
            transform.rotate.entry[2][1] = 0.032614365f;
            transform.rotate.entry[2][2] = 0.999392152f;
            return transform;
        }

        RE::NiPoint3 Normalize(
            const RE::NiPoint3& a_vector,
            const RE::NiPoint3& a_fallback)
        {
            const float lengthSquared =
                a_vector.x * a_vector.x +
                a_vector.y * a_vector.y +
                a_vector.z * a_vector.z;
            if (lengthSquared <= 1.0e-6f) {
                return a_fallback;
            }
            return a_vector / std::sqrt(lengthSquared);
        }

        float Dot(const RE::NiPoint3& a_left, const RE::NiPoint3& a_right)
        {
            return a_left.x * a_right.x + a_left.y * a_right.y + a_left.z * a_right.z;
        }

        RE::NiPoint3 ResolveHmdForward(RE::PlayerCharacter& a_player)
        {
            auto* vrData = a_player.GetVRNodeData();
            auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
            if (!hmd) {
                const auto yaw = a_player.GetAngleZ();
                return { -std::sin(yaw), std::cos(yaw), 0.0f };
            }
            return Normalize(
                { hmd->world.rotate.entry[0][1], hmd->world.rotate.entry[1][1], 0.0f },
                { 0.0f, 1.0f, 0.0f });
        }
    }

    RE::NiTransform CalibratedHandToGlider(bool a_isLeft)
    {
        RE::NiTransform transform{};
        transform.scale = 1.176471f;
        if (a_isLeft) {
            transform.translate = { -15.662109f, -30.972656f, 17.057617f };
            transform.rotate.entry[0][0] = -0.334620357f;
            transform.rotate.entry[0][1] = 0.902088463f;
            transform.rotate.entry[0][2] = 0.272516280f;
            transform.rotate.entry[1][0] = -0.902805030f;
            transform.rotate.entry[1][1] = -0.389777184f;
            transform.rotate.entry[1][2] = 0.181702793f;
            transform.rotate.entry[2][0] = 0.270132720f;
            transform.rotate.entry[2][1] = -0.185227498f;
            transform.rotate.entry[2][2] = 0.944837868f;
        } else {
            transform.translate = { 7.255859f, -0.496582f, 8.871094f };
            transform.rotate.entry[0][0] = -0.449148178f;
            transform.rotate.entry[0][1] = -0.868204474f;
            transform.rotate.entry[0][2] = -0.210918888f;
            transform.rotate.entry[1][0] = 0.893406689f;
            transform.rotate.entry[1][1] = -0.438945770f;
            transform.rotate.entry[1][2] = -0.095661044f;
            transform.rotate.entry[2][0] = -0.009528650f;
            transform.rotate.entry[2][1] = -0.231402293f;
            transform.rotate.entry[2][2] = 0.972811162f;
        }
        return transform;
    }

    bool ResolveGliderSpawnFrame(
        RE::PlayerCharacter& a_player,
        GliderSpawnFrame& a_frame)
    {
        auto* vrData = a_player.GetVRNodeData();
        auto* left = vrData ? vrData->LeftWandNode.get() : nullptr;
        auto* right = vrData ? vrData->RightWandNode.get() : nullptr;
        auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
        if (!left || !right || !hmd) {
            return false;
        }

        a_frame.leftHand = left->world.translate;
        a_frame.rightHand = right->world.translate;
        a_frame.hmdForward = ResolveHmdForward(a_player);
        a_frame.world = hmd->world * CalibratedHmdToGlider();
        a_frame.world.scale = 1.0f;
        a_frame.localX = {
            a_frame.world.rotate.entry[0][0],
            a_frame.world.rotate.entry[1][0],
            a_frame.world.rotate.entry[2][0]
        };
        a_frame.forward = {
            a_frame.world.rotate.entry[0][1],
            a_frame.world.rotate.entry[1][1],
            a_frame.world.rotate.entry[2][1]
        };
        a_frame.up = {
            a_frame.world.rotate.entry[0][2],
            a_frame.world.rotate.entry[1][2],
            a_frame.world.rotate.entry[2][2]
        };
        return true;
    }

    bool ApplyGliderSpawnFrame(
        RE::TESObjectREFR& a_reference,
        const GliderSpawnFrame& a_frame)
    {
        RE::NiPoint3 angles{};
        a_frame.world.rotate.ToEulerAnglesXYZ(angles);
        a_reference.SetScale(1.0f);
        a_reference.SetPosition(a_frame.world.translate);
        a_reference.SetAngle(angles);

        auto* root = a_reference.Get3D();
        if (!root) {
            root = a_reference.Load3D(false);
        }
        if (!root) {
            return false;
        }

        root->local = root->parent ? root->parent->world.Invert() * a_frame.world : a_frame.world;
        RE::NiUpdateData updateData{};
        updateData.flags = RE::NiUpdateData::Flag::kDirty;
        root->UpdateWorldData(std::addressof(updateData));
        return true;
    }

    float GetHmdForwardAlignment(const GliderSpawnFrame& a_frame)
    {
        return Dot(a_frame.forward, a_frame.hmdForward);
    }

    void LogCalibrationTransform(
        std::string_view a_label,
        const RE::NiTransform& a_transform)
    {
        RE::NiPoint3 angles{};
        a_transform.rotate.ToEulerAnglesXYZ(angles);
        constexpr float radiansToDegrees = 57.29577951308232f;
        logger::info(
            "CALIBRATION {} translate=({:.6f},{:.6f},{:.6f}) eulerDegrees=({:.6f},{:.6f},{:.6f}) scale={:.6f} matrix=[{:.9f},{:.9f},{:.9f};{:.9f},{:.9f},{:.9f};{:.9f},{:.9f},{:.9f}]",
            a_label,
            a_transform.translate.x,
            a_transform.translate.y,
            a_transform.translate.z,
            angles.x * radiansToDegrees,
            angles.y * radiansToDegrees,
            angles.z * radiansToDegrees,
            a_transform.scale,
            a_transform.rotate.entry[0][0],
            a_transform.rotate.entry[0][1],
            a_transform.rotate.entry[0][2],
            a_transform.rotate.entry[1][0],
            a_transform.rotate.entry[1][1],
            a_transform.rotate.entry[1][2],
            a_transform.rotate.entry[2][0],
            a_transform.rotate.entry[2][1],
            a_transform.rotate.entry[2][2]);
    }
}

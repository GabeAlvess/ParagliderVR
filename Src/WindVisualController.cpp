#include "pch.h"
#include "WindVisualController.h"
#include "Config.h"

#include <RE/B/BSModelDB.h>

namespace ParagliderVR
{
    namespace
    {
        RE::NiPoint3 Normalize(
            const RE::NiPoint3& a_value,
            const RE::NiPoint3& a_fallback = {})
        {
            const float length = a_value.Length();
            return length > 0.0001f ? a_value / length : a_fallback;
        }

        RE::NiPoint3 Cross(const RE::NiPoint3& a_first, const RE::NiPoint3& a_second)
        {
            return {
                (a_first.y * a_second.z) - (a_first.z * a_second.y),
                (a_first.z * a_second.x) - (a_first.x * a_second.z),
                (a_first.x * a_second.y) - (a_first.y * a_second.x)
            };
        }

        RE::NiMatrix3 BuildRotation(const RE::NiPoint3& a_forward)
        {
            const RE::NiPoint3 up{ 0.0f, 0.0f, 1.0f };
            const auto right = Normalize(Cross(a_forward, up), { 1.0f, 0.0f, 0.0f });
            const auto forward = Normalize(Cross(up, right), { 0.0f, 1.0f, 0.0f });
            RE::NiMatrix3 rotation{};
            rotation.entry[0][0] = right.x;
            rotation.entry[1][0] = right.y;
            rotation.entry[2][0] = right.z;
            rotation.entry[0][1] = forward.x;
            rotation.entry[1][1] = forward.y;
            rotation.entry[2][1] = forward.z;
            rotation.entry[0][2] = up.x;
            rotation.entry[1][2] = up.y;
            rotation.entry[2][2] = up.z;
            return rotation;
        }

        void SetWorldTransform(
            RE::NiAVObject& a_object,
            const RE::NiNode& a_parent,
            const RE::NiPoint3& a_position,
            const RE::NiMatrix3& a_rotation,
            float a_scale)
        {
            const float parentScale = (std::max)(a_parent.world.scale, 0.0001f);
            const auto inverseParentRotation = a_parent.world.rotate.Transpose();
            a_object.local.translate =
                inverseParentRotation * (a_position - a_parent.world.translate) / parentScale;
            a_object.local.rotate = inverseParentRotation * a_rotation;
            a_object.local.scale = a_scale / parentScale;
        }

        RE::NiNode* ResolveThirdPersonRoot(RE::PlayerCharacter& a_player)
        {
            auto* thirdPerson = a_player.Get3D(false);
            if (!thirdPerson) {
                return nullptr;
            }
            if (auto* root = thirdPerson->GetObjectByName("NPC Root [Root]"); root) {
                return root->AsNode();
            }
            return thirdPerson->AsNode();
        }

        RE::NiPointer<RE::NiNode> LoadModel(const std::string& a_path)
        {
            RE::NiPointer<RE::NiNode> modelRoot;
            RE::BSModelDB::DBTraits::ArgsType arguments{};
            const auto result = RE::BSModelDB::Demand(a_path.c_str(), modelRoot, arguments);
            if (result != RE::BSResource::ErrorCode::kNone || !modelRoot) {
                logger::error(
                    "Wind visual model load failed path='{}' error={}",
                    a_path,
                    static_cast<int>(result));
                return nullptr;
            }
            auto* clone = modelRoot->Clone();
            auto* node = clone ? clone->AsNode() : nullptr;
            if (!node) {
                logger::error("Wind visual model clone is not a NiNode path='{}'", a_path);
                return nullptr;
            }
            node->collisionObject = nullptr;
            node->SetAppCulled(true);
            std::size_t geometryCount = 0;
            RE::BSVisit::TraverseScenegraphGeometries(
                node,
                [&geometryCount](RE::BSGeometry* a_geometry) {
                    if (!a_geometry) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    ++geometryCount;
                    a_geometry->SetAppCulled(false);
                    if (auto* property = a_geometry->lightingShaderProp_cast()) {
                        property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kTwoSided);
                        property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kNoFade);
                        property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kZBufferTest);
                        property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kZBufferWrite);
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            logger::info(
                "Wind visual model loaded path='{}' geometries={} twoSided=true",
                a_path,
                geometryCount);
            return RE::NiPointer<RE::NiNode>(node);
        }
    }

    void WindVisualController::Update(
        RE::PlayerCharacter& a_player,
        const FlightCommand& a_flightCommand)
    {
        Ensure(a_player);
        if (!_visual || !_parent) {
            return;
        }

        const auto& settings = Config::GetSingleton().Get();
        const auto forward = Normalize(
            { a_flightCommand.steeringDirection.x, a_flightCommand.steeringDirection.y, 0.0f },
            { 0.0f, 1.0f, 0.0f });
        constexpr float degreesToRadians = 0.017453292519943295f;
        RE::NiMatrix3 adjustment{};
        adjustment.SetEulerAnglesXYZ(
            settings.windVisualRotationDegrees.x * degreesToRadians,
            settings.windVisualRotationDegrees.y * degreesToRadians,
            settings.windVisualRotationDegrees.z * degreesToRadians);
        const auto position = a_player.GetPosition() +
            RE::NiPoint3{ 0.0f, 0.0f, settings.windVisualHeight } +
            (forward * settings.windVisualForward);
        const float speedRange =
            (std::max)(settings.steeringSpeed - settings.minimumForwardSpeed, 1.0f);
        const float speedRatio = std::clamp(
            (a_flightCommand.steeringSpeed - settings.minimumForwardSpeed) / speedRange,
            0.0f,
            1.0f);
        const float scale = settings.windVisualScale * (0.75f + (0.35f * speedRatio));
        SetWorldTransform(
            *_visual,
            *_parent,
            position,
            BuildRotation(forward) * adjustment,
            scale);
        RE::NiUpdateData updateData;
        updateData.flags = RE::NiUpdateData::Flag::kDirty;
        _visual->Update(updateData);
        if (!_transformLogged) {
            _transformLogged = true;
            logger::info(
                "Wind visual transform path='{}' position=({:.1f},{:.1f},{:.1f}) scale={:.2f} speedRatio={:.2f}",
                settings.windVisualModelPath,
                position.x,
                position.y,
                position.z,
                scale,
                speedRatio);
        }
    }

    void WindVisualController::Hide()
    {
        if (_visual) {
            _visual->SetAppCulled(true);
            if (_visual->parent) {
                _visual->parent->DetachChild(_visual.get());
            }
        }
        _parent = nullptr;
        _transformLogged = false;
    }

    void WindVisualController::Ensure(RE::PlayerCharacter& a_player)
    {
        const auto& settings = Config::GetSingleton().Get();
        if (!settings.windVisualEnabled) {
            Hide();
            return;
        }
        auto* thirdPersonRoot = ResolveThirdPersonRoot(a_player);
        if (!thirdPersonRoot) {
            return;
        }
        if (!_visual) {
            _visual = LoadModel(settings.windVisualModelPath);
        }
        if (!_visual) {
            return;
        }
        if (_parent != thirdPersonRoot) {
            if (_visual->parent) {
                _visual->parent->DetachChild(_visual.get());
            }
            thirdPersonRoot->AttachChild(_visual.get(), true);
            _parent = thirdPersonRoot;
            _transformLogged = false;
            logger::info(
                "Wind visual attached to third-person node name='{}'",
                thirdPersonRoot->name.c_str());
        }
        _visual->SetAppCulled(false);
    }
}

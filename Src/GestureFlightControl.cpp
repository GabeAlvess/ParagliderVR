#include "pch.h"
#include "Config.h"
#include "GestureFlightControl.h"

namespace ParagliderVR
{
    namespace
    {
        constexpr float kDegreesToRadians = 0.01745329251994329577f;

        struct HandPoseReference
        {
            RE::NiPoint3 position{};
            std::array<float, 9> rotation{};
        };

        struct GesturePoseReference
        {
            std::array<HandPoseReference, 2> hands{};
            float verticalThrottle = 0.0f;
            float horizontalThrottle = 0.0f;
            float lateralThrottle = 0.0f;
        };

        const std::array<GesturePoseReference, 6> kGesturePoses{
            GesturePoseReference{
                .hands = {
                    HandPoseReference{ { -9.343750f, 12.817383f, 8.856445f }, { 0.935882568f, 0.352284908f, -0.004379451f, 0.230465412f, -0.621563315f, -0.748695374f, -0.266476154f, 0.699681640f, -0.662899733f } },
                    HandPoseReference{ { 12.869141f, 11.247070f, 9.517578f }, { 0.924745262f, -0.373693168f, -0.072109103f, -0.287557542f, -0.561932206f, -0.775591969f, 0.249312967f, 0.737960517f, -0.627102315f } }
                },
                .horizontalThrottle = 1.0f
            },
            GesturePoseReference{
                .hands = {
                    HandPoseReference{ { -9.396484f, -7.839844f, 5.303467f }, { 0.893341899f, 0.445321381f, -0.060242727f, 0.446248055f, -0.894906580f, 0.002176663f, -0.052942302f, -0.028827712f, -0.998181283f } },
                    HandPoseReference{ { 10.015625f, -6.632812f, 6.711426f }, { 0.892942905f, -0.418264836f, 0.166455388f, -0.409501255f, -0.908287764f, -0.085569620f, 0.186980143f, 0.008245088f, -0.982329190f } }
                },
                .horizontalThrottle = -1.0f
            },
            GesturePoseReference{
                .hands = {
                    HandPoseReference{ { -16.878906f, 1.195312f, 3.540283f }, { 0.796600282f, 0.515859902f, 0.315145135f, 0.603865623f, -0.655068517f, -0.454127371f, -0.027824473f, 0.552063346f, -0.833337843f } },
                    HandPoseReference{ { 1.365234f, 8.510742f, 12.561523f }, { 0.772068620f, -0.463585436f, 0.434739888f, -0.132078826f, -0.786153913f, -0.603752971f, 0.621663392f, 0.408718795f, -0.668194532f } }
                },
                .lateralThrottle = -1.0f
            },
            GesturePoseReference{
                .hands = {
                    HandPoseReference{ { -6.289062f, 3.671875f, 13.225830f }, { 0.826123595f, 0.547913253f, -0.131571040f, 0.412986338f, -0.747589052f, -0.520147085f, -0.383356541f, 0.375368685f, -0.843881547f } },
                    HandPoseReference{ { 16.074219f, 2.814453f, 3.152100f }, { 0.948179424f, -0.243411824f, -0.204221636f, -0.317719728f, -0.732730925f, -0.601796985f, -0.003155001f, 0.635496795f, -0.772097170f } }
                },
                .lateralThrottle = 1.0f
            },
            GesturePoseReference{
                .hands = {
                    HandPoseReference{ { -11.857422f, -1.025391f, -0.053223f }, { 0.852370322f, 0.510435641f, 0.113667950f, 0.517359495f, -0.791447461f, -0.325499743f, -0.076184444f, 0.336253494f, -0.938685060f } },
                    HandPoseReference{ { 12.076172f, 0.150391f, 0.577637f }, { 0.933206320f, -0.357392669f, 0.037371323f, -0.303327829f, -0.839227915f, -0.451318979f, 0.192661092f, 0.409837902f, -0.891579986f } }
                },
                .verticalThrottle = 1.0f
            },
            GesturePoseReference{
                .hands = {
                    HandPoseReference{ { -13.390625f, 4.957031f, 23.842041f }, { 0.904288173f, 0.411353290f, 0.114243150f, 0.381999731f, -0.660139322f, -0.646755159f, -0.190628469f, 0.628493845f, -0.754093051f } },
                    HandPoseReference{ { 11.679688f, 2.879883f, 23.350098f }, { 0.916352212f, -0.400343418f, -0.004878491f, -0.311908573f, -0.706186950f, -0.635620415f, 0.251021296f, 0.583973765f, -0.771986485f } }
                },
                .verticalThrottle = -1.0f
            }
        };

        float RotationDifferenceRadians(
            const RE::NiMatrix3& a_current,
            const std::array<float, 9>& a_reference)
        {
            float matrixProductTrace = 0.0f;
            for (std::size_t row = 0; row < 3; ++row) {
                for (std::size_t column = 0; column < 3; ++column) {
                    matrixProductTrace +=
                        a_current.entry[row][column] * a_reference[(row * 3) + column];
                }
            }
            return std::acos(std::clamp(
                (matrixProductTrace - 1.0f) * 0.5f,
                -1.0f,
                1.0f));
        }

        float GestureConfidence(
            const std::array<RE::NiTransform, 2>& a_handsInHmd,
            const GesturePoseReference& a_reference,
            const Settings& a_settings)
        {
            const float positionTolerance = (std::max)(a_settings.gesturePositionTolerance, 0.001f);
            const float rotationTolerance = (std::max)(
                a_settings.gestureRotationToleranceDegrees * kDegreesToRadians,
                0.001f);
            float normalizedErrorSquared = 0.0f;
            for (std::size_t index = 0; index < a_handsInHmd.size(); ++index) {
                const float positionError =
                    (a_handsInHmd[index].translate - a_reference.hands[index].position).Length() /
                    positionTolerance;
                const float rotationError = RotationDifferenceRadians(
                    a_handsInHmd[index].rotate,
                    a_reference.hands[index].rotation) /
                    rotationTolerance;
                normalizedErrorSquared +=
                    (positionError * positionError) + (rotationError * rotationError);
            }
            const float normalizedError = std::sqrt(normalizedErrorSquared * 0.25f);
            const float rawConfidence = std::clamp(1.0f - normalizedError, 0.0f, 1.0f);
            if (rawConfidence <= a_settings.gestureMinimumConfidence) {
                return 0.0f;
            }
            const float remapped =
                (rawConfidence - a_settings.gestureMinimumConfidence) /
                (1.0f - a_settings.gestureMinimumConfidence);
            return remapped * remapped * (3.0f - (2.0f * remapped));
        }

        float SmoothToward(float a_current, float a_target, float a_speed, float a_delta)
        {
            const float blend = 1.0f - std::exp(-(std::max)(a_speed, 0.0f) * a_delta);
            return a_current + ((a_target - a_current) * blend);
        }
    }

    void GestureFlightControl::Reset()
    {
        _verticalThrottle = 0.0f;
        _horizontalThrottle = 0.0f;
        _lateralThrottle = 0.0f;
    }

    GestureFlightControlSample GestureFlightControl::Update(
        const RE::NiAVObject& a_hmd,
        const ParagliderInputState& a_input,
        bool a_dualHanded,
        float a_delta,
        const Settings& a_settings)
    {
        GestureFlightControlSample sample{};
        const GesturePoseReference* selectedPose = nullptr;
        if (a_settings.gestureControlEnabled && a_dualHanded &&
            a_input.hands[0].valid && a_input.hands[1].valid) {
            const auto inverseHmd = a_hmd.world.Invert();
            const std::array handsInHmd{
                inverseHmd * a_input.hands[0].worldTransform,
                inverseHmd * a_input.hands[1].worldTransform
            };
            for (std::size_t index = 0; index < kGesturePoses.size(); ++index) {
                const float confidence = GestureConfidence(
                    handsInHmd,
                    kGesturePoses[index],
                    a_settings);
                if (confidence > sample.confidence) {
                    sample.confidence = confidence;
                    sample.gestureIndex = static_cast<int>(index);
                    selectedPose = std::addressof(kGesturePoses[index]);
                }
            }
        }

        const float targetVertical = selectedPose ?
            selectedPose->verticalThrottle * sample.confidence : 0.0f;
        const float targetHorizontal = selectedPose ?
            selectedPose->horizontalThrottle * sample.confidence : 0.0f;
        const float targetLateral = selectedPose ?
            selectedPose->lateralThrottle * sample.confidence : 0.0f;
        _verticalThrottle = SmoothToward(
            _verticalThrottle,
            targetVertical,
            a_settings.gestureTransitionSpeed,
            a_delta);
        _horizontalThrottle = SmoothToward(
            _horizontalThrottle,
            targetHorizontal,
            a_settings.gestureTransitionSpeed,
            a_delta);
        _lateralThrottle = SmoothToward(
            _lateralThrottle,
            targetLateral,
            a_settings.gestureTransitionSpeed,
            a_delta);
        sample.verticalThrottle = _verticalThrottle;
        sample.horizontalThrottle = _horizontalThrottle;
        sample.lateralThrottle = _lateralThrottle;
        return sample;
    }
}

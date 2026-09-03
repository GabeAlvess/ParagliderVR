#pragma once

#include "ParagliderInput.h"

namespace ParagliderVR
{
    struct Settings;

    struct GestureFlightControlSample
    {
        float verticalThrottle = 0.0f;
        float horizontalThrottle = 0.0f;
        float lateralThrottle = 0.0f;
        float confidence = 0.0f;
        int gestureIndex = -1;
    };

    class GestureFlightControl final
    {
    public:
        void Reset();
        [[nodiscard]] GestureFlightControlSample Update(
            const RE::NiAVObject& a_hmd,
            const ParagliderInputState& a_input,
            bool a_dualHanded,
            float a_delta,
            const Settings& a_settings);

    private:
        float _verticalThrottle = 0.0f;
        float _horizontalThrottle = 0.0f;
        float _lateralThrottle = 0.0f;
    };
}

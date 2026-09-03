#pragma once

namespace ParagliderVR::AnimationOar
{
    enum class Direction
    {
        kIdle,
        kForward,
        kBackward,
        kLeft,
        kRight
    };

    void RegisterCondition();
    void SetActive(bool a_active);
    void SetDirection(Direction a_direction);
}

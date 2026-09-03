#pragma once

#include "FlightDynamics.h"

namespace ParagliderVR
{
    class WindVisualController final
    {
    public:
        void Update(RE::PlayerCharacter& a_player, const FlightCommand& a_flightCommand);
        void Hide();

    private:
        void Ensure(RE::PlayerCharacter& a_player);

        RE::NiPointer<RE::NiNode> _visual;
        RE::NiNode* _parent = nullptr;
        bool _transformLogged = false;
    };
}

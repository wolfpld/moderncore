#pragma once

#include "util/Vector2.hpp"

struct WaylandScroll
{
    enum class Source
    {
        Unknown,
        Wheel,
        Finger,
        Continuous,
        Tilt
    };

    Source source;
    Vector2<float> delta;
    Vector2<bool> inverted;
};

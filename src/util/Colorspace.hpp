#pragma once

#include <lcms2.h>

inline constexpr cmsCIExyY white709 = { 0.3127f, 0.329f, 1 };
inline constexpr cmsCIExyYTRIPLE primaries709 = {
    { 0.64f, 0.33f, 1 },
    { 0.30f, 0.60f, 1 },
    { 0.15f, 0.06f, 1 }
};

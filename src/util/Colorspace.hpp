#pragma once

#include <lcms2.h>

enum class Colorspace
{
    BT709,
    BT2020
};

inline constexpr cmsCIExyY white709 = { 0.3127f, 0.329f, 1 };
inline constexpr cmsCIExyYTRIPLE primaries709 = {
    { 0.64f, 0.33f, 1 },
    { 0.30f, 0.60f, 1 },
    { 0.15f, 0.06f, 1 }
};
inline constexpr cmsCIExyYTRIPLE primaries2020 = {
    { 0.708f, 0.292f, 1 },
    { 0.170f, 0.797f, 1 },
    { 0.131f, 0.046f, 1 }
};
inline constexpr cmsCIExyY whiteDciP3 = { 0.314f, 0.351f, 1 };
inline constexpr cmsCIExyYTRIPLE primariesDciP3 = {
    { 0.680f, 0.320f, 1 },
    { 0.265f, 0.690f, 1 },
    { 0.150f, 0.060f, 1 }
};

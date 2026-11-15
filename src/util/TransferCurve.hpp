#pragma once

#include <lcms2.h>

#include "Logs.hpp"

// Input tone curve for a coded transfer characteristic per ITU-T H.273 (07/24),
// Table 3, as used by nclx/cICP color profiles. Values with no dedicated lcms2
// representation fall back to sRGB, and unspecified (2) likewise per the
// still-image convention. The caller owns the returned curve (cmsFreeToneCurve).
static inline cmsToneCurve* BuildTransferToneCurve( int transfer, const char* loaderName )
{
    cmsFloat64Number srgbParams[] = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045 };
    cmsFloat64Number bt709Params[] = { 1.0 / 0.45, 1.0 / 1.099, 0.099 / 1.099, 1.0 / 4.5, 0.081 };

    switch( transfer )
    {
    case 2: // unspecified: conventionally sRGB for still images
    case 11: // IEC 61966-2-4 (sYCC): parametric representation identical to sRGB
    case 13:
        return cmsBuildParametricToneCurve( nullptr, 4, srgbParams );
    case 1:
    case 6:
    case 14:
    case 15:
        return cmsBuildParametricToneCurve( nullptr, 4, bt709Params );
    case 4:
        return cmsBuildGamma( nullptr, 2.2 );
    case 5:
        return cmsBuildGamma( nullptr, 2.8 );
    case 8:
        return cmsBuildGamma( nullptr, 1.0 );
    default:
        mclog( LogLevel::Error, "%s transfer characteristic %d not implemented, assuming sRGB", loaderName, transfer );
        return cmsBuildParametricToneCurve( nullptr, 4, srgbParams );
    }
}

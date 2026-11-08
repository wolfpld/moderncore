#pragma once

#include <lcms2.h>
#include <stddef.h>
#include <stdint.h>

#include "util/NoCopy.hpp"

class TaskDispatch;

inline constexpr cmsUInt32Number IccRenderIntent = INTENT_PERCEPTUAL;

class FastIccTransform
{
public:
    FastIccTransform( cmsHPROFILE profileIn, cmsHPROFILE profileOut );
    NoCopy( FastIccTransform );

    explicit operator bool() const { return m_valid; }
    void Run( TaskDispatch* td, const uint8_t* input, float* output, size_t pixels ) const;

private:
    float m_lut[3][256];
    float m_mat[9];
    bool m_valid = false;
};

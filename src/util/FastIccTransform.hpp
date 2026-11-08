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

    bool Run( TaskDispatch* td, uint8_t* input, float* output, size_t pixels ) const;
    bool RunFloat( TaskDispatch* td, const float* input, float* output, size_t pixels ) const;
    bool RunHalf( TaskDispatch* td, uint16_t* data, size_t pixels ) const;

    explicit operator bool() const { return m_valid; }
    bool SupportsFloat() const { return m_valid && m_linear; }

private:
    float m_lut[3][256];
    float m_mat[9];
    bool m_valid = false;
    bool m_linear = false;
};

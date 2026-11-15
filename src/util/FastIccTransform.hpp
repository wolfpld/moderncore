#pragma once

#include <lcms2.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "util/NoCopy.hpp"

class TaskDispatch;

inline constexpr cmsUInt32Number IccRenderIntent = INTENT_PERCEPTUAL;

class FastIccTransform
{
public:
    FastIccTransform( cmsHPROFILE profileIn, cmsHPROFILE profileOut );
    ~FastIccTransform();
    NoCopy( FastIccTransform );

    bool Run( TaskDispatch* td, uint8_t* input, float* output, size_t pixels ) const;
    bool RunFloat( TaskDispatch* td, const float* input, float* output, size_t pixels ) const;
    bool RunHalf( TaskDispatch* td, uint16_t* data, size_t pixels ) const;

    bool PrepareCoded( int bits );
    bool RunCodedToU8( TaskDispatch* td, const float* input, uint8_t* output, size_t pixels ) const;

    explicit operator bool() const { return m_valid; }
    bool SupportsFloat() const { return m_valid && m_linear; }

private:
    float m_lut[3][256];
    float m_mat[9];
    bool m_valid = false;
    bool m_linear = false;
    bool m_encodeValid = false;
    bool m_ready = false;
    cmsToneCurve* m_inTrc[3] = {};
    cmsToneCurve* m_outTrc[3] = {};
    std::vector<float> m_codedLut;
    std::vector<uint8_t> m_encodeLut;
    int m_codedBits = 0;
};

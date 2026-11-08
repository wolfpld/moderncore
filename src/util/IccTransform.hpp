#pragma once

#include <lcms2.h>
#include <stddef.h>
#include <stdint.h>

#include "util/FastIccTransform.hpp"
#include "util/NoCopy.hpp"

class TaskDispatch;

class IccTransform
{
public:
    IccTransform( cmsHPROFILE profileIn, cmsHPROFILE profileOut );
    ~IccTransform();
    NoCopy( IccTransform );

    bool Run( TaskDispatch* td, uint8_t* input, float* output, size_t pixels ) const;
    bool RunFloat( TaskDispatch* td, const float* input, float* output, size_t pixels ) const;
    bool RunHalf( TaskDispatch* td, uint16_t* data, size_t pixels ) const;

private:
    enum Kind
    {
        u8ToFloat,
        floatToFloat,
        halfToHalf,
        recoverToSrgb,
        srgbToFloat,
        count,
    };

    bool RunLcms( Kind kind, TaskDispatch* td, const void* input, void* output, size_t pixels, size_t inStride, size_t outStride ) const;

    FastIccTransform m_fast;
    cmsHTRANSFORM m_transforms[count] = {};
    cmsHPROFILE m_srgb = nullptr;
};

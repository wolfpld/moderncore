#pragma once

#include <algorithm>
#include <lcms2.h>
#include <stddef.h>
#include <stdint.h>

#include "util/FastIccTransform.hpp"
#include "util/NoCopy.hpp"
#include "TaskDispatch.hpp"

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

template<typename TInput, typename TOutput>
static inline void CmsTransform( TaskDispatch* td, cmsHTRANSFORM transform, const TInput* input, TOutput* output, size_t size )
{
    if( !td )
    {
        cmsDoTransform( transform, input, output, size );
    }
    else
    {
        while( size > 0 )
        {
            auto chunk = std::min<size_t>( size, 16 * 1024 );
            td->Queue( [input, output, chunk, transform] {
                cmsDoTransform( transform, input, output, chunk );
            } );
            input += chunk * 4;
            output += chunk * 4;
            size -= chunk;
        }
        td->Sync();
    }
}

#include "FastIccTransform.hpp"

#include "util/TaskDispatch.hpp"

#include <algorithm>
#include <immintrin.h>

#include "contrib/half.hpp"

namespace
{
bool HasLinearTrc( cmsHPROFILE profile )
{
    const cmsTagSignature trcTags[3] = { cmsSigRedTRCTag, cmsSigGreenTRCTag, cmsSigBlueTRCTag };
    for( size_t c = 0; c < 3; c++ )
    {
        if( !cmsIsTag( profile, trcTags[c] ) ) continue;
        const cmsToneCurve* trc = (const cmsToneCurve*)cmsReadTag( profile, trcTags[c] );
        if( !trc || !cmsIsToneCurveLinear( (cmsToneCurve*)trc ) ) return false;
    }
    return true;
}

#ifdef __AVX512F__
struct Coeffs
{
    __m512 c0, c1, c2;
    __m512i i0, i1, i2;
};

Coeffs MakeCoeffs( const float* mat )
{
    Coeffs k;
    auto tile = []( float a, float b, float c )
    {
        return _mm512_setr_ps( a, b, c, 0, a, b, c, 0, a, b, c, 0, a, b, c, 0 );
    };
    k.c0 = tile( mat[0], mat[3], mat[6] );
    k.c1 = tile( mat[1], mat[4], mat[7] );
    k.c2 = tile( mat[2], mat[5], mat[8] );
    k.i0 = _mm512_setr_epi32( 0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12 );
    k.i1 = _mm512_setr_epi32( 1, 1, 1, 1, 5, 5, 5, 5, 9, 9, 9, 9, 13, 13, 13, 13 );
    k.i2 = _mm512_setr_epi32( 2, 2, 2, 2, 6, 6, 6, 6, 10, 10, 10, 10, 14, 14, 14, 14 );
    return k;
}
#endif

void FloatRange( const float* in, float* out, size_t n, const float* mat )
{
#ifdef __AVX512F__
    const Coeffs k = MakeCoeffs( mat );
    while( n >= 4 )
    {
        const __m512 v = _mm512_loadu_ps( in );
        __m512 o = _mm512_mul_ps( _mm512_permutexvar_ps( k.i0, v ), k.c0 );
        o = _mm512_fmadd_ps( _mm512_permutexvar_ps( k.i1, v ), k.c1, o );
        o = _mm512_fmadd_ps( _mm512_permutexvar_ps( k.i2, v ), k.c2, o );
        _mm512_storeu_ps( out, _mm512_mask_blend_ps( 0x8888, o, v ) );
        in += 16;
        out += 16;
        n -= 4;
    }
#endif
    for( size_t i = 0; i < n; i++ )
    {
        const float r = in[0], g = in[1], b = in[2];
        out[0] = mat[0]*r + mat[1]*g + mat[2]*b;
        out[1] = mat[3]*r + mat[4]*g + mat[5]*b;
        out[2] = mat[6]*r + mat[7]*g + mat[8]*b;
        out[3] = in[3];
        in += 4;
        out += 4;
    }
}

void HalfRange( uint16_t* data, size_t n, const float* mat )
{
#ifdef __AVX512F__
    const Coeffs k = MakeCoeffs( mat );
    while( n >= 4 )
    {
        const __m512 v = _mm512_cvtph_ps( _mm256_loadu_si256( (const __m256i*)data ) );
        __m512 o = _mm512_mul_ps( _mm512_permutexvar_ps( k.i0, v ), k.c0 );
        o = _mm512_fmadd_ps( _mm512_permutexvar_ps( k.i1, v ), k.c1, o );
        o = _mm512_fmadd_ps( _mm512_permutexvar_ps( k.i2, v ), k.c2, o );
        o = _mm512_mask_blend_ps( 0x8888, o, v );
        _mm256_storeu_si256( (__m256i*)data, _mm512_cvtps_ph( o, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC ) );
        data += 16;
        n -= 4;
    }
#endif
    for( size_t i = 0; i < n; i++ )
    {
        auto px = (half_float::half*)data;
        const float r = float( px[0] ), g = float( px[1] ), b = float( px[2] );
        px[0] = half_float::half( mat[0]*r + mat[1]*g + mat[2]*b );
        px[1] = half_float::half( mat[3]*r + mat[4]*g + mat[5]*b );
        px[2] = half_float::half( mat[6]*r + mat[7]*g + mat[8]*b );
        data += 4;
    }
}

bool GetColorantMatrix( cmsHPROFILE profile, cmsFloat64Number out[9] )
{
    const cmsCIEXYZ* xyz[3] = {
        (const cmsCIEXYZ*)cmsReadTag( profile, cmsSigRedColorantTag ),
        (const cmsCIEXYZ*)cmsReadTag( profile, cmsSigGreenColorantTag ),
        (const cmsCIEXYZ*)cmsReadTag( profile, cmsSigBlueColorantTag )
    };
    for( auto p : xyz )
    {
        if( !p ) return false;
    }
    out[0] = xyz[0]->X; out[1] = xyz[1]->X; out[2] = xyz[2]->X;
    out[3] = xyz[0]->Y; out[4] = xyz[1]->Y; out[5] = xyz[2]->Y;
    out[6] = xyz[0]->Z; out[7] = xyz[1]->Z; out[8] = xyz[2]->Z;
    return true;
}

void FastShaperRange( const uint8_t* in, float* out, size_t n, const float (*lut)[256], const float* mat )
{
#if defined __SSE4_1__ && defined __FMA__
#ifdef __AVX512F__
    const __m512 zero = _mm512_setzero_ps(), one = _mm512_set1_ps( 1.f );
    const __m512i lo8 = _mm512_set1_epi32( 0xff );
    const __m512 m00 = _mm512_set1_ps( mat[0] ), m01 = _mm512_set1_ps( mat[1] ), m02 = _mm512_set1_ps( mat[2] );
    const __m512 m10 = _mm512_set1_ps( mat[3] ), m11 = _mm512_set1_ps( mat[4] ), m12 = _mm512_set1_ps( mat[5] );
    const __m512 m20 = _mm512_set1_ps( mat[6] ), m21 = _mm512_set1_ps( mat[7] ), m22 = _mm512_set1_ps( mat[8] );
    const __m512i idxOut = _mm512_setr_epi32( 0, 1, 16, 20, 4, 5, 17, 21, 8, 9, 18, 22, 12, 13, 19, 23 );
    const __m512i idxB[4] = {
        _mm512_setr_epi32( 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 ),
        _mm512_setr_epi32( 4, 5, 6, 7, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 ),
        _mm512_setr_epi32( 8, 9, 10, 11, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 ),
        _mm512_setr_epi32( 12, 13, 14, 15, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 )
    };
    __m512i idxRG[4];
    for( int j = 0; j < 4; j++ )
    {
        const int b = j * 4;
        idxRG[j] = _mm512_setr_epi32( b, 16+b, 1, 1, b+1, 16+b+1, 1, 1, b+2, 16+b+2, 1, 1, b+3, 16+b+3, 1, 1 );
    }

    while( n >= 16 )
    {
        const __m512i px = _mm512_loadu_si512( in );
        const __m512 r = _mm512_i32gather_ps( _mm512_and_si512( px, lo8 ), lut[0], 4 );
        const __m512 g = _mm512_i32gather_ps( _mm512_and_si512( _mm512_srli_epi32( px, 8 ), lo8 ), lut[1], 4 );
        const __m512 b = _mm512_i32gather_ps( _mm512_and_si512( _mm512_srli_epi32( px, 16 ), lo8 ), lut[2], 4 );

        __m512 x = _mm512_fmadd_ps( m00, r, _mm512_fmadd_ps( m01, g, _mm512_mul_ps( m02, b ) ) );
        __m512 y = _mm512_fmadd_ps( m10, r, _mm512_fmadd_ps( m11, g, _mm512_mul_ps( m12, b ) ) );
        __m512 z = _mm512_fmadd_ps( m20, r, _mm512_fmadd_ps( m21, g, _mm512_mul_ps( m22, b ) ) );
        x = _mm512_min_ps( _mm512_max_ps( x, zero ), one );
        y = _mm512_min_ps( _mm512_max_ps( y, zero ), one );
        z = _mm512_min_ps( _mm512_max_ps( z, zero ), one );

        for( int j = 0; j < 4; j++ )
        {
            const __m512 rg = _mm512_permutex2var_ps( x, idxRG[j], y );
            const __m512 bo = _mm512_mask_blend_ps( 0x00f0, _mm512_permutexvar_ps( idxB[j], z ), one );
            _mm512_storeu_ps( out + j*16, _mm512_permutex2var_ps( rg, idxOut, bo ) );
        }
        in += 64;
        out += 64;
        n -= 16;
    }
#endif

#ifdef __AVX2__
    const __m256 fone = _mm256_set1_ps( 1.f ), fzero = _mm256_setzero_ps();
    const __m256i lo8x = _mm256_set1_epi32( 0xff );
    const __m256 k00 = _mm256_set1_ps( mat[0] ), k01 = _mm256_set1_ps( mat[1] ), k02 = _mm256_set1_ps( mat[2] );
    const __m256 k10 = _mm256_set1_ps( mat[3] ), k11 = _mm256_set1_ps( mat[4] ), k12 = _mm256_set1_ps( mat[5] );
    const __m256 k20 = _mm256_set1_ps( mat[6] ), k21 = _mm256_set1_ps( mat[7] ), k22 = _mm256_set1_ps( mat[8] );
    const __m256 alpha = _mm256_blend_ps( fzero, fone, 0x88 );
    const __m256i maskR = _mm256_setr_epi32( -1, 0, 0, 0, -1, 0, 0, 0 );
    const __m256i maskG = _mm256_setr_epi32( 0, -1, 0, 0, 0, -1, 0, 0 );
    const __m256i maskB = _mm256_setr_epi32( 0, 0, -1, 0, 0, 0, -1, 0 );

    while( n >= 8 )
    {
        const __m256i px = _mm256_loadu_si256( (const __m256i*)in );
        const __m256 r = _mm256_i32gather_ps( lut[0], _mm256_and_si256( px, lo8x ), 4 );
        const __m256 g = _mm256_i32gather_ps( lut[1], _mm256_and_si256( _mm256_srli_epi32( px, 8 ), lo8x ), 4 );
        const __m256 b = _mm256_i32gather_ps( lut[2], _mm256_and_si256( _mm256_srli_epi32( px, 16 ), lo8x ), 4 );

        __m256 x = _mm256_fmadd_ps( k00, r, _mm256_fmadd_ps( k01, g, _mm256_mul_ps( k02, b ) ) );
        __m256 y = _mm256_fmadd_ps( k10, r, _mm256_fmadd_ps( k11, g, _mm256_mul_ps( k12, b ) ) );
        __m256 z = _mm256_fmadd_ps( k20, r, _mm256_fmadd_ps( k21, g, _mm256_mul_ps( k22, b ) ) );
        x = _mm256_min_ps( _mm256_max_ps( x, fzero ), fone );
        y = _mm256_min_ps( _mm256_max_ps( y, fzero ), fone );
        z = _mm256_min_ps( _mm256_max_ps( z, fzero ), fone );

        for( int q = 0; q < 4; q++ )
        {
            const __m256i ir = _mm256_setr_epi32( 2*q, 0, 0, 0, 2*q+1, 0, 0, 0 );
            const __m256i ig = _mm256_setr_epi32( 0, 2*q, 0, 0, 0, 2*q+1, 0, 0 );
            const __m256i ib = _mm256_setr_epi32( 0, 0, 2*q, 0, 0, 0, 2*q+1, 0 );
            const __m256 v = _mm256_or_ps( alpha,
                _mm256_or_ps( _mm256_and_ps( _mm256_permutevar8x32_ps( x, ir ), _mm256_castsi256_ps( maskR ) ),
                    _mm256_or_ps( _mm256_and_ps( _mm256_permutevar8x32_ps( y, ig ), _mm256_castsi256_ps( maskG ) ),
                        _mm256_and_ps( _mm256_permutevar8x32_ps( z, ib ), _mm256_castsi256_ps( maskB ) ) ) ) );
            _mm256_storeu_ps( out + q*8, v );
        }
        in += 32;
        out += 32;
        n -= 8;
    }
#endif
#endif

    for( size_t i = 0; i < n; i++ )
    {
        const float r = lut[0][in[0]], g = lut[1][in[1]], b = lut[2][in[2]];
        in += 4;
        for( int k = 0; k < 3; k++ )
        {
            auto v = mat[k*3]*r + mat[k*3+1]*g + mat[k*3+2]*b;
            if( v < 0.f ) v = 0.f;
            else if( v > 1.f ) v = 1.f;
            *out++ = v;
        }
        *out++ = 1.f;
    }
}
}

FastIccTransform::FastIccTransform( cmsHPROFILE profileIn, cmsHPROFILE profileOut )
{
    if( !profileIn || !profileOut || !cmsIsMatrixShaper( profileIn ) || !cmsIsMatrixShaper( profileOut ) ||
        cmsIsCLUT( profileIn, IccRenderIntent, LCMS_USED_AS_INPUT ) ||
        cmsIsCLUT( profileOut, IccRenderIntent, LCMS_USED_AS_OUTPUT ) ||
        cmsChannelsOf( cmsGetColorSpace( profileIn ) ) != 3 ||
        !HasLinearTrc( profileOut ) )
    {
        return;
    }

    const cmsTagSignature trcTags[3] = { cmsSigRedTRCTag, cmsSigGreenTRCTag, cmsSigBlueTRCTag };
    for( size_t c = 0; c < 3; c++ )
    {
        const cmsToneCurve* trc = nullptr;
        if( cmsIsTag( profileIn, trcTags[c] ) )
        {
            trc = (const cmsToneCurve*)cmsReadTag( profileIn, trcTags[c] );
            if( !trc ) return;
        }
        for( int i = 0; i < 256; i++ )
        {
            m_lut[c][i] = trc ? static_cast<float>( cmsEvalToneCurveFloat( trc, i / 255.0 ) ) : i / 255.f;
        }
    }

    m_linear = HasLinearTrc( profileIn ) && HasLinearTrc( profileOut );

    cmsFloat64Number mr[9], mo[9];
    if( !GetColorantMatrix( profileIn, mr ) || !GetColorantMatrix( profileOut, mo ) ) return;

    const auto det = mo[0] * ( mo[4] * mo[8] - mo[5] * mo[7] ) -
        mo[1] * ( mo[3] * mo[8] - mo[5] * mo[6] ) +
        mo[2] * ( mo[3] * mo[7] - mo[4] * mo[6] );
    if( det < 1e-9 && det > -1e-9 ) return;

    cmsFloat64Number inv[9] = {
        ( mo[4] * mo[8] - mo[5] * mo[7] ) / det, ( mo[2] * mo[7] - mo[1] * mo[8] ) / det, ( mo[1] * mo[5] - mo[2] * mo[4] ) / det,
        ( mo[5] * mo[6] - mo[3] * mo[8] ) / det, ( mo[0] * mo[8] - mo[2] * mo[6] ) / det, ( mo[2] * mo[3] - mo[0] * mo[5] ) / det,
        ( mo[3] * mo[7] - mo[4] * mo[6] ) / det, ( mo[1] * mo[6] - mo[0] * mo[7] ) / det, ( mo[0] * mo[4] - mo[1] * mo[3] ) / det
    };
    for( int r = 0; r < 3; r++ )
    {
        for( int c = 0; c < 3; c++ )
        {
            m_mat[r*3+c] = static_cast<float>(
                inv[r*3] * mr[c] + inv[r*3+1] * mr[3+c] + inv[r*3+2] * mr[6+c] );
        }
    }
    m_valid = true;
}

bool FastIccTransform::Run( TaskDispatch* td, uint8_t* input, float* output, size_t pixels ) const
{
    if( !m_valid ) return false;

    if( !td )
    {
        FastShaperRange( input, output, pixels, m_lut, m_mat );
    }
    else
    {
        auto size = pixels;
        while( size > 0 )
        {
            auto chunk = std::min<size_t>( size, 256 * 1024 );
            td->Queue( [this, input, output, chunk] {
                FastShaperRange( input, output, chunk, m_lut, m_mat );
            } );
            input += chunk * 4;
            output += chunk * 4;
            size -= chunk;
        }
        td->Sync();
    }
    return true;
}

bool FastIccTransform::RunFloat( TaskDispatch* td, const float* input, float* output, size_t pixels ) const
{
    if( !SupportsFloat() ) return false;

    if( !td )
    {
        FloatRange( input, output, pixels, m_mat );
    }
    else
    {
        auto size = pixels;
        while( size > 0 )
        {
            auto chunk = std::min<size_t>( size, 256 * 1024 );
            td->Queue( [this, input, output, chunk] {
                FloatRange( input, output, chunk, m_mat );
            } );
            input += chunk * 4;
            output += chunk * 4;
            size -= chunk;
        }
        td->Sync();
    }
    return true;
}

bool FastIccTransform::RunHalf( TaskDispatch* td, uint16_t* data, size_t pixels ) const
{
    if( !SupportsFloat() ) return false;

    if( !td )
    {
        HalfRange( data, pixels, m_mat );
    }
    else
    {
        auto size = pixels;
        while( size > 0 )
        {
            auto chunk = std::min<size_t>( size, 256 * 1024 );
            td->Queue( [this, data, chunk] {
                HalfRange( data, chunk, m_mat );
            } );
            data += chunk * 4;
            size -= chunk;
        }
        td->Sync();
    }
    return true;
}

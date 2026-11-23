#ifndef MIPGEN_WEIGHTS_H
#define MIPGEN_WEIGHTS_H

#ifdef __cplusplus
#include <cmath>
#endif

#define MIPGEN_TOO_SMALL 1.4901161193847656e-08
#define MIPGEN_ALPHA_TOO_SMALL 7.523163845247995e-37

int Gcd( int a, int b )
{
    while( b != 0 )
    {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

float KernelDown( float x )
{
    if( x < 0.0 ) x = -x;
    if( x < 1.0 ) return ( 5.333333492279 + x * x * ( 7.0 * x - 12.0 ) ) / 6.0;
    if( x < 2.0 ) return ( 10.666666984558 + x * ( -20.0 + x * ( 12.0 - 2.33333325386 * x ) ) ) / 6.0;
    return 0.0;
}

float KernelUp( float x )
{
    if( x < 0.0 ) x = -x;
    if( x < 1.0 ) return 1.0 - x * x * ( 2.5 - 1.5 * x );
    if( x < 2.0 ) return 2.0 - x * ( 4.0 + x * ( 0.5 * x - 2.5 ) );
    return 0.0;
}

int Num( int srcSize, int dstSize )
{
    return dstSize / Gcd( srcSize, dstSize );
}

int Den( int srcSize, int dstSize )
{
    return srcSize / Gcd( srcSize, dstSize );
}

float TapX( int n, int d, int o, int i )
{
    return float( ( 2 * o + 1 ) * d - ( 2 * i + 1 ) * n ) / float( 2 * d );
}

float TapWeight( int n, int d, int o, int i, int down )
{
    float w = down != 0 ? KernelDown( TapX( n, d, o, i ) ) : KernelUp( TapX( n, d, o, i ) );
    if( w < MIPGEN_TOO_SMALL && w > -MIPGEN_TOO_SMALL ) return 0.0;
    return w * float( n ) / float( d );
}

float SrgbEncode( float c )
{
    if( c <= 0.0031308 ) return c * 12.92;
    return 1.055 * pow( c, 1.0 / 2.4 ) - 0.055;
}

#endif

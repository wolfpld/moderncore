#include <cmath>

#include "Tonemapper.hpp"
#include "util/Panic.hpp"

namespace ToneMap
{

void Process( Operator op, uint32_t* dst, float* src, size_t sz )
{
    switch( op )
    {
    case Operator::AgX:
        AgX( dst, src, sz );
        break;
    case Operator::AgXGolden:
        AgXGolden( dst, src, sz );
        break;
    case Operator::AgXPunchy:
        AgXPunchy( dst, src, sz );
        break;
    case Operator::PbrNeutral:
        PbrNeutral( dst, src, sz );
        break;
    default:
        Panic( "Invalid tonemapper operator %d", int( op ) );
    }
}

float LinearToSrgb( float x )
{
    if( x <= 0.0031308f ) return 12.92f * x;
    return 1.055f * std::pow( x, 1.0f / 2.4f ) - 0.055f;
}

}

vec3 Pq( vec3 color )
{
    const float NominalLuminance = 100.0;

    const float m1 = 1305.0 / 8192.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 107.0 / 128.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;

    const vec3 Y = color / ( 10000.0 / NominalLuminance );
    const vec3 Ym1 = pow( Y, vec3( m1 ) );

    return pow( ( c1 + c2 * Ym1 ) / ( 1.0 + c3 * Ym1 ), vec3( m2 ) );
}

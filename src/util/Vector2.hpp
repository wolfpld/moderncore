#pragma once

#include <string.h>

template<class T>
struct Vector2
{
    Vector2() : x( 0 ), y( 0 ) {}
    explicit Vector2( T v ) : x( v ), y( v ) {}
    Vector2( T _x, T _y ) : x( _x ), y( _y ) {}
    template<class Y>
    explicit Vector2( const Vector2<Y>& v ) : x( T( v.x ) ), y( T( v.y ) ) {}

    bool operator==( const Vector2<T>& rhs ) const { return memcmp( this, &rhs, sizeof( rhs ) ) == 0; }
    bool operator!=( const Vector2<T>& rhs ) const { return !( *this == rhs ); }

    Vector2<T>& operator+=( const Vector2<T>& rhs )
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    Vector2<T>& operator-=( const Vector2<T>& rhs )
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    Vector2<T>& operator*=( const Vector2<T>& rhs )
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    T x, y;
};

template<class T>
Vector2<T> operator+( const Vector2<T>& lhs, const Vector2<T>& rhs )
{
    return Vector2<T>( lhs.x + rhs.x, lhs.y + rhs.y );
}

template<class T>
Vector2<T> operator-( const Vector2<T>& lhs, const Vector2<T>& rhs )
{
    return Vector2<T>( lhs.x - rhs.x, lhs.y - rhs.y );
}

template<class T, class Y>
Vector2<T> operator*( const Vector2<T>& lhs, const Y& rhs )
{
    return Vector2<T>( lhs.x * rhs, lhs.y * rhs );
}

template<class T, class Y>
Vector2<T> operator/( const Vector2<T>& lhs, const Y& rhs )
{
    return Vector2<T>( lhs.x / rhs, lhs.y / rhs );
}

template<class T>
bool operator<( const Vector2<T>& lhs, const Vector2<T>& rhs )
{
    return lhs.x == rhs.x ? lhs.y < rhs.y : lhs.x < rhs.x;
}

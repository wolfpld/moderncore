#ifndef __NOCOPY_HPP__
#define __NOCOPY_HPP__

#define NoCopy( T ) \
    T( const T& ) = delete; \
    T& operator=( const T& ) = delete

#endif

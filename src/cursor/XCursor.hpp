#ifndef __XCURSOR_HPP__
#define __XCURSOR_HPP__

#include "CursorBase.hpp"

class XCursor : public CursorBase
{
public:
    explicit XCursor( const char* theme );
};

#endif

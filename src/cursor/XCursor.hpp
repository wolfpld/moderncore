#ifndef __XCURSOR_HPP__
#define __XCURSOR_HPP__

#include "CursorBaseMulti.hpp"

class XCursor : public CursorBaseMulti
{
public:
    explicit XCursor( const char* theme );
};

#endif

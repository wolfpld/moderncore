#ifndef __WINCURSOR_HPP__
#define __WINCURSOR_HPP__

#include "CursorBaseMulti.hpp"

class WinCursor : public CursorBaseMulti
{
public:
    explicit WinCursor( const char* theme );
};

#endif

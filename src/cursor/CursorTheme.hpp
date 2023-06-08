#ifndef __CURSORTHEME_HPP__
#define __CURSORTHEME_HPP__

#include <memory>

class CursorBase;

class CursorTheme
{
public:
    CursorTheme();
    ~CursorTheme();

private:
    std::unique_ptr<CursorBase> m_cursor;
    uint32_t m_size;
};

#endif

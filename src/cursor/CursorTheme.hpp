#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "util/NoCopy.hpp"

class CursorBase;

class CursorTheme
{
public:
    struct CursorException : public std::runtime_error { explicit CursorException( const std::string& msg ) : std::runtime_error( msg ) {} };

    CursorTheme();
    ~CursorTheme();

    NoCopy( CursorTheme );

    [[nodiscard]] const CursorBase& Cursor() const { return *m_cursor; }
    [[nodiscard]] uint32_t Size() const { return m_size; }

private:
    std::unique_ptr<CursorBase> m_cursor;
    uint32_t m_size;
};

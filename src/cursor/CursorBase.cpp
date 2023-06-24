#include <algorithm>

#include "CursorBase.hpp"

const CursorData* CursorBase::Get( uint32_t size, CursorType type ) const
{
    auto it = m_cursor.find( size );
    if( it == m_cursor.end() ) return nullptr;
    auto& t = it->second.type[(int)type];
    if( t.bitmaps.empty() ) return &it->second.type[(int)CursorType::Default];
    return &t;
}

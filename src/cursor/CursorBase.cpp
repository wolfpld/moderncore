#include "CursorBase.hpp"

const std::vector<CursorBitmap>* CursorBase::Get( uint32_t size, CursorType type )
{
    auto it = m_cursor.find( size );
    if( it == m_cursor.end() ) return nullptr;
    auto& t = it->second.type[(int)type];
    if( t.empty() ) return &it->second.type[(int)CursorType::Default];
    return &t;
}

#include <algorithm>
#include <assert.h>

#include "CursorBase.hpp"

const std::vector<CursorBitmap>* CursorBase::Get( uint32_t size, CursorType type )
{
    auto it = m_cursor.find( size );
    if( it == m_cursor.end() ) return nullptr;
    auto& t = it->second.type[(int)type];
    if( t.empty() ) return &it->second.type[(int)CursorType::Default];
    return &t;
}

void CursorBase::CalcSizes()
{
    assert( !m_cursor.empty() );
    assert( m_sizes.empty() );

    for( auto& v : m_cursor ) m_sizes.emplace_back( v.first );
    std::sort( m_sizes.begin(), m_sizes.end() );
}

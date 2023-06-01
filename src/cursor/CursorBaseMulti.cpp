#include <algorithm>
#include <assert.h>

#include "CursorBaseMulti.hpp"

uint32_t CursorBaseMulti::FitSize( uint32_t size )
{
    assert( !m_cursor.empty() );
    assert( !m_sizes.empty() );

    const auto mit = m_cursor.find( size );
    if( mit != m_cursor.end() ) return size;

    if( size < m_sizes.front() ) return m_sizes.front();
    if( size > m_sizes.back() ) return m_sizes.back();

    const auto it = std::lower_bound( m_sizes.begin() + 1, m_sizes.end(), size );
    const auto next = *it;
    const auto prev = *(it-1);
    const auto dnext = next - size;
    const auto dprev = size - prev;
    return dnext > dprev ? prev : next;
}

void CursorBaseMulti::CalcSizes()
{
    assert( !m_cursor.empty() );
    assert( m_sizes.empty() );

    for( auto& v : m_cursor ) m_sizes.emplace_back( v.first );
    std::sort( m_sizes.begin(), m_sizes.end() );
}

#include <algorithm>
#include <ranges>

#include "CursorBaseMulti.hpp"
#include "util/Panic.hpp"

uint32_t CursorBaseMulti::FitSize( uint32_t size ) const
{
    CheckPanic( !m_cursor.empty(), "Cursor is empty" );
    CheckPanic( !m_sizes.empty(), "Sizes are not calculated" );

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
    CheckPanic( !m_cursor.empty(), "Cursor is empty" );
    CheckPanic( m_sizes.empty(), "Sizes are already calculated" );

    for( auto& v : m_cursor ) m_sizes.emplace_back( v.first );
    std::ranges::sort( m_sizes );
}

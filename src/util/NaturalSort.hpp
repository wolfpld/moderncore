#pragma once

#include <cstddef>
#include <string_view>

// Natural-order string comparator approximating localized numeric collation:
//  - digit runs compare by numeric value, arbitrary length, never parsed;
//    runs of equal value ( "01" vs "1" ) order by their raw bytes;
//  - other bytes compare case-insensitively (ASCII) and by unsigned byte value
//    beyond that, so UTF-8 names stay above ASCII punctuation and digits exactly as
//    a plain byte sort left them; case-folded pairs ( "A" vs "a" ) fall back to a
//    byte-wise compare, keeping the order total and therefore valid for std::sort.
// '.' deliberately does NOT join digit runs across it: value comparison of
// dot-separated groups is not transitive.
struct NaturalLess
{
    bool operator()( std::string_view a, std::string_view b ) const
    {
        size_t i = 0;
        size_t j = 0;
        while( i < a.size() && j < b.size() )
        {
            const bool da = IsDigit( a[i] );
            const bool db = IsDigit( b[j] );
            if( da && db )
            {
                const std::string_view ra = TakeDigitRun( a, i );
                const std::string_view rb = TakeDigitRun( b, j );
                if( const int r = CompareNumeric( ra, rb ) ) return r < 0;
                continue; // CompareNumeric already settled value and leading-zero ties; the runs are byte-identical here
            }

            const unsigned char ca = Lower( a[i] );
            const unsigned char cb = Lower( b[j] );
            if( ca != cb ) return ca < cb;
            i++;
            j++;
        }

        // Ties consume key-identical content (modulo case folds), so one side exhausting makes its key a
        // prefix of the other's: shorter key first. A byte-wise compare here would
        // again disagree with the folded order ( "a" < "a0" < "A1" < "a" ).
        if( i < a.size() || j < b.size() ) return i >= a.size();
        return a < b; // equal keys; only case ties can differ, settle byte-wise
    }

private:
    static constexpr bool IsDigit( unsigned char c ) { return c >= '0' && c <= '9'; }
    // Unsigned, so bytes above 0x7F keep the same order the byte-wise fallback gives them.
    static constexpr unsigned char Lower( unsigned char c ) { return ( c >= 'A' && c <= 'Z' ) ? c + 32 : c; }

    static constexpr std::string_view TakeDigitRun( std::string_view s, size_t& i )
    {
        const size_t start = i;
        while( i < s.size() && IsDigit( s[i] ) ) i++;
        return s.substr( start, i - start );
    }

    // Three-way compare of digit runs by value: strip leading zeros, longer run wins,
    // equal lengths compare lexicographically. Value ties settle on the raw run, right
    // here: deferring them to the global byte-wise fallback would let the aligned
    // continuation decide one way and the fallback the other, losing transitivity
    // ( "1" < "1." < "01A" < "1" ).
    static constexpr int CompareNumeric( std::string_view a, std::string_view b )
    {
        const auto na = StripLeadingZeros( a );
        const auto nb = StripLeadingZeros( b );
        if( na.size() != nb.size() ) return na.size() < nb.size() ? -1 : 1;
        if( na != nb ) return na < nb ? -1 : 1;
        if( a != b ) return a < b ? -1 : 1;
        return 0;
    }

    static constexpr std::string_view StripLeadingZeros( std::string_view d )
    {
        size_t k = 0;
        while( d.size() - k > 1 && d[k] == '0' ) k++;
        return d.substr( k );
    }
};

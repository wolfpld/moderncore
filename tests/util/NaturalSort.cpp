#include <catch2/catch_all.hpp>
#include <src/util/NaturalSort.hpp>
#include <algorithm>
#include <random>
#include <string>
#include <vector>

static std::vector<std::string> Sorted( std::vector<std::string> v )
{
    std::ranges::sort( v, NaturalLess{} );
    return v;
}

TEST_CASE( "NaturalLess orders digit runs by value", "[naturalsort]" )
{
    SECTION( "Numbers sort arithmetically, not lexically" )
    {
        REQUIRE( Sorted( { "IMG_10.jpg", "IMG_2.jpg", "IMG_1.jpg", "IMG_11.jpg" } )
                 == std::vector<std::string>{ "IMG_1.jpg", "IMG_2.jpg", "IMG_10.jpg", "IMG_11.jpg" } );
    }

    SECTION( "Digit runs longer than 64 bits compare correctly" )
    {
        REQUIRE( NaturalLess{}( "f99999999999999999999", "f100000000000000000000" ) );
    }

    SECTION( "Run vs. text aligns like numeric-mode collation" )
    {
        REQUIRE( NaturalLess{}( "1a", "12" ) ); // 1 < 12, decided before 'a' is reached
    }

    SECTION( "Leading zeros tie, settled byte-wise for determinism" )
    {
        REQUIRE( NaturalLess{}( "IMG_01", "IMG_1" ) );
        REQUIRE_FALSE( NaturalLess{}( "IMG_1", "IMG_01" ) );
    }
}

TEST_CASE( "NaturalLess case folding", "[naturalsort]" )
{
    REQUIRE( Sorted( { "Zebra.png", "apple.png", "Banana.png" } )
             == std::vector<std::string>{ "apple.png", "Banana.png", "Zebra.png" } );

    // Case-equal pairs fall back to byte-wise order: upper case before lower case.
    REQUIRE( NaturalLess{}( "Photo.png", "photo.png" ) );
}

TEST_CASE( "NaturalLess treats dots as ordinary characters", "[naturalsort]" )
{
    // Not decimal semantics: groups compare as separate runs ( 1.2 before 1.10 ).
    REQUIRE( Sorted( { "v1.10.png", "v1.2.png", "v1.1.png" } )
             == std::vector<std::string>{ "v1.1.png", "v1.2.png", "v1.10.png" } );

    // Punctuation below and above digits interleaves by byte order.
    REQUIRE( Sorted( { "a9", "a!", "a." } ) == std::vector<std::string>{ "a!", "a.", "a9" } );
}

TEST_CASE( "NaturalLess degenerate inputs", "[naturalsort]" )
{
    REQUIRE_FALSE( NaturalLess{}( "", "" ) );
    REQUIRE( NaturalLess{}( "", "a" ) );
    REQUIRE( NaturalLess{}( "a", "a1" ) ); // prefix first
    REQUIRE( NaturalLess{}( "000", "1" ) );
    REQUIRE_FALSE( NaturalLess{}( "00", "0" ) ); // equal value, byte-wise order
}

TEST_CASE( "NaturalLess is a strict weak ordering", "[naturalsort]" )
{
    // Exhaustive over all strings up to length 3 from an alphabet that mixes
    // digits, case pairs, and punctuation on both sides of the digit range.
    static constexpr char alphabet[] = { '0', '1', 'A', 'a', '.', '!' };
    std::vector<std::string> words{ std::string() };
    std::vector<std::string> frontier{ std::string() };
    for( size_t level = 0; level < 3; level++ )
    {
        std::vector<std::string> next;
        next.reserve( frontier.size() * std::size( alphabet ) );
        for( const auto& w : frontier )
            for( char c : alphabet )
                next.push_back( w + c );
        frontier.swap( next );
        words.insert( words.end(), frontier.begin(), frontier.end() );
    }

    NaturalLess less;
    const auto equiv = [&]( const std::string& x, const std::string& y )
    {
        return !less( x, y ) && !less( y, x );
    };

    for( const auto& a : words )
    {
        REQUIRE_FALSE( less( a, a ) ); // irreflexive
        for( const auto& b : words )
            REQUIRE_FALSE( ( less( a, b ) && less( b, a ) ) ); // asymmetric
    }

    // Transitivity of < and of equivalence: the strict-weak-ordering contract.
    for( const auto& a : words )
        for( const auto& b : words )
        {
            if( !less( a, b ) ) continue;
            for( const auto& c : words )
                if( less( b, c ) ) REQUIRE( less( a, c ) );
        }

    for( const auto& a : words )
        for( const auto& b : words )
        {
            if( !equiv( a, b ) ) continue;
            for( const auto& c : words )
                if( equiv( b, c ) ) REQUIRE( equiv( a, c ) );
        }

    // Equality classes are singletons: only byte-identical strings compare equal.
    for( size_t i = 0; i < words.size(); i++ )
        for( size_t j = i + 1; j < words.size(); j++ )
            REQUIRE_FALSE( equiv( words[i], words[j] ) );
}

TEST_CASE( "NaturalLess sorts longer strings safely", "[naturalsort]" )
{
    // Fixed-seed fuzz: sorting must terminate, keep every element, and produce
    // output that is sorted under the same comparator; results must match a
    // stable sort, which only holds when equivalent elements are identical.
    static constexpr char alphabet[] = { '0', '1', '2', '9', 'a', 'A', '.', '_', '-', 'z' };
    std::mt19937 rng( 20261115 );
    std::uniform_int_distribution<int> lenDist( 0, 24 );

    std::vector<std::string> words;
    for( int n = 0; n < 2000; n++ )
    {
        std::string s;
        const int len = lenDist( rng );
        for( int i = 0; i < len; i++ )
            s += alphabet[rng() % std::size( alphabet )];
        words.push_back( std::move( s ) );
    }

    auto a = words;
    auto b = words;
    std::ranges::sort( a, NaturalLess{} );
    std::ranges::stable_sort( b, NaturalLess{} );

    REQUIRE( a.size() == words.size() );
    REQUIRE( std::ranges::is_sorted( a, NaturalLess{} ) );
    REQUIRE( a == b ); // unstable and stable order agree: equality classes hold identical strings

    auto byteSorted = words;
    std::ranges::sort( byteSorted );
    std::ranges::sort( a );
    REQUIRE( a == byteSorted ); // sorting preserved the multiset
}

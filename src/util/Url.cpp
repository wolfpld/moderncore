#include "Url.hpp"

void UrlDecode( std::string& url )
{
    size_t pos = 0;
    while( ( pos = url.find( '%', pos ) ) != std::string::npos )
    {
        if( pos + 2 >= url.size() )
        {
            url.erase( pos );
            break;
        }
        const auto c = std::stoi( url.substr( pos + 1, 2 ), nullptr, 16 );
        url.replace( pos, 3, 1, c );
        pos += 1;
    }
}

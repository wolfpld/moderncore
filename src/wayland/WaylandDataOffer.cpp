#include "WaylandDataOffer.hpp"
#include "util/Invoke.hpp"

WaylandDataOffer::WaylandDataOffer( wl_data_offer* offer )
    : m_offer( offer )
{
    static constexpr wl_data_offer_listener listener = {
        .offer = Method( DataOffer ),
        .source_actions = Method( DataSourceActions ),
        .action = Method( DataAction )
    };

    wl_data_offer_add_listener( offer, &listener, this );
}

WaylandDataOffer::~WaylandDataOffer()
{
    wl_data_offer_destroy( m_offer );
}

void WaylandDataOffer::DataOffer( wl_data_offer* offer, const char* mimeType )
{
    m_mimeTypes.emplace( mimeType );
}

void WaylandDataOffer::DataSourceActions( wl_data_offer* offer, uint32_t sourceActions )
{
}

void WaylandDataOffer::DataAction( wl_data_offer* offer, uint32_t dndAction )
{
}

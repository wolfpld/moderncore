#pragma once

#include <string>
#include <wayland-client.h>

#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"

class WaylandDataOffer
{
public:
    WaylandDataOffer( wl_data_offer* offer );
    ~WaylandDataOffer();

    [[nodiscard]] const auto& MimeTypes() const { return m_mimeTypes; }

    operator wl_data_offer*() const { return m_offer; }

    NoCopy( WaylandDataOffer );

private:
    void DataOffer( wl_data_offer* offer, const char* mimeType );
    void DataSourceActions( wl_data_offer* offer, uint32_t sourceActions );
    void DataAction( wl_data_offer* offer, uint32_t dndAction );

    wl_data_offer* m_offer;
    unordered_flat_set<std::string> m_mimeTypes;
};

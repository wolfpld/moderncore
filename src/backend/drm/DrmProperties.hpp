#pragma once

#include <stdint.h>
#include <xf86drmMode.h>

#include "util/NoCopy.hpp"

class DrmProperty
{
public:
    DrmProperty() : DrmProperty( 0, nullptr, 0 ) {}
    explicit DrmProperty( int fd, drmModePropertyPtr prop, uint64_t value );
    ~DrmProperty();

    NoCopy( DrmProperty );

    drmModePropertyBlobPtr Blob();

    operator bool() const { return m_fd != 0; }

private:
    int m_fd;
    uint64_t m_value;
    drmModePropertyPtr m_prop;
    drmModePropertyBlobPtr m_blob;
};


class DrmProperties
{
public:
    DrmProperties( int fd, uint32_t id, uint32_t type );
    ~DrmProperties();

    NoCopy( DrmProperties );

    void List() const;

    [[nodiscard]] DrmProperty operator[]( const char* name ) const;

    operator bool() const { return m_props; }

private:
    int m_fd;
    drmModeObjectPropertiesPtr m_props;
};

#include <assert.h>

#include "DataDeviceManager.hpp"
#include "Display.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_data_device.h>
};

DataDeviceManager::DataDeviceManager( const Display& dpy )
    : m_ddm( wlr_data_device_manager_create( dpy ) )
{
    assert( m_ddm );
}

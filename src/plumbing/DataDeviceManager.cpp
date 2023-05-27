#include "DataDeviceManager.hpp"
#include "Display.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_data_device.h>
};

DataDeviceManager::DataDeviceManager( const Display& dpy )
    : m_ddm( wlr_data_device_manager_create( dpy ) )
{
    CheckPanic( m_ddm, "Failed to create wlr_data_device_manager!" );
}

#ifndef __DATADEVICEMANAGER_HPP__
#define __DATADEVICEMANAGER_HPP__

extern "C" {
    struct wlr_data_device_manager;
};

class Display;

class DataDeviceManager
{
public:
    explicit DataDeviceManager( const Display& dpy );

    operator wlr_data_device_manager* () const { return m_ddm; }

private:
    struct wlr_data_device_manager* m_ddm;
};

#endif

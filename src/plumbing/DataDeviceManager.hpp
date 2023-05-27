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

    [[nodiscard]] operator wlr_data_device_manager* () const { return m_ddm; }

private:
    wlr_data_device_manager* m_ddm;
};

#endif

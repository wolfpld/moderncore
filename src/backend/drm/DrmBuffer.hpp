#pragma once

#include <xf86drmMode.h>

class DrmDevice;

class DrmBuffer
{
public:
    DrmBuffer( DrmDevice& device, const drmModeModeInfo& mode );
    ~DrmBuffer();

private:
    DrmDevice& m_device;
};

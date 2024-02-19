#include "DrmBuffer.hpp"

DrmBuffer::DrmBuffer( DrmDevice& device, const drmModeModeInfo& mode )
    : m_device( device )
{
}

DrmBuffer::~DrmBuffer()
{
}

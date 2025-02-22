#pragma once

#include <memory>
#include <vector>

class VlkBase;
class VlkFence;

class GarbageChute
{
public:
    ~GarbageChute() = default;

    virtual void Recycle( std::shared_ptr<VlkBase>&& garbage ) = 0;
    virtual void Recycle( std::vector<std::shared_ptr<VlkBase>>&& garbage ) = 0;
};

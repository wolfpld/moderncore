#pragma once

#include <memory>
#include <vector>

#include "VlkBase.hpp"

class VlkFence;

class VlkGarbage
{
    struct Garbage
    {
        std::shared_ptr<VlkFence> fence;
        std::vector<std::shared_ptr<VlkBase>> objects;
    };

public:
    ~VlkGarbage();

    void Recycle( std::shared_ptr<VlkFence> fence, std::vector<std::shared_ptr<VlkBase>>&& objects );
    void Collect();

private:
    std::vector<Garbage> m_garbage;
};

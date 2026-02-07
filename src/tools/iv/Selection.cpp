#include "Selection.hpp"

Selection::Selection( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format )
    : m_garbage( garbage )
    , m_device( std::move( device ) )
{
}

Selection::~Selection()
{
}

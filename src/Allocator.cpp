#include "Allocator.hpp"
#include "Backend.hpp"
#include "Panic.hpp"
#include "Renderer.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/render/allocator.h>
};

Allocator::Allocator( const Backend& backend, const Renderer& renderer )
    : m_allocator( wlr_allocator_autocreate( backend, renderer ) )
{
    CheckPanic( m_allocator, "Failed to create wlr_allocator!" );
}

Allocator::~Allocator()
{
    wlr_allocator_destroy( m_allocator );
}
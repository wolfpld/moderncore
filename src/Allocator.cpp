#include <assert.h>

#include "Allocator.hpp"
#include "Backend.hpp"
#include "Renderer.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/render/allocator.h>
};

Allocator::Allocator( const Backend& backend, const Renderer& renderer )
    : m_allocator( wlr_allocator_autocreate( backend, renderer ) )
{
    assert( m_allocator );
}

Allocator::~Allocator()
{
    wlr_allocator_destroy( m_allocator );
}
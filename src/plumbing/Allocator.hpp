#ifndef __WLR_ALLOCATOR_HPP__
#define __WLR_ALLOCATOR_HPP__

extern "C" {
    struct wlr_allocator;
};

class BackendWlr;
class Renderer;

class Allocator
{
public:
    Allocator( const BackendWlr& backend, const Renderer& renderer );
    ~Allocator();

    [[nodiscard]] operator wlr_allocator* () const { return m_allocator; }

private:
    wlr_allocator* m_allocator;
};

#endif

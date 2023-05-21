#ifndef __ALLOCATOR_HPP__
#define __ALLOCATOR_HPP__

extern "C" {
    struct wlr_allocator;
};

class Backend;
class Renderer;

class Allocator
{
public:
    Allocator( const Backend& backend, const Renderer& renderer );
    ~Allocator();

    [[nodiscard]] operator wlr_allocator* () const { return m_allocator; }

private:
    wlr_allocator* m_allocator;
};

#endif

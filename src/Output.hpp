#ifndef __OUTPUT_HPP__
#define __OUTPUT_HPP__

#include <memory>
#include <vector>

#include "Listener.hpp"

extern "C" {
    struct wlr_output;
    struct wlr_output_layout;
};

class Allocator;
class Backend;
class OutputNode;
class Renderer;
class Scene;

class Output
{
public:
    explicit Output( const Backend& backend, const Allocator& allocator, const Renderer& renderer, Scene& scene );
    ~Output();

    [[nodiscard]] wlr_output_layout* GetLayout() const { return m_layout; }

private:
    void NewOutput( wlr_output* output );

    const Allocator& m_allocator;
    const Renderer& m_renderer;
    Scene& m_scene;

    wlr_output_layout* m_layout;
    Listener<wlr_output> m_newOutput;

    std::vector<std::unique_ptr<OutputNode>> m_nodes;
};

#endif

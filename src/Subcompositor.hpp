#ifndef __SUBCOMPOSITOR_HPP__
#define __SUBCOMPOSITOR_HPP__

extern "C" {
    struct wlr_subcompositor;
};

class Display;

class Subcompositor
{
public:
    explicit Subcompositor( const Display& dpy );

    [[nodiscard]] operator wlr_subcompositor* () const { return m_subcompositor; }

private:
    wlr_subcompositor* m_subcompositor;
};

#endif

#ifndef __WLR_BACKEND_HPP__
#define __WLR_BACKEND_HPP__

extern "C" {
    struct wlr_backend;
};

class Display;

class BackendWlr
{
public:
    explicit BackendWlr( const Display& dpy );
    ~BackendWlr();

    void Start();

    [[nodiscard]] wlr_backend* Get() const { return m_backend; }

    [[nodiscard]] operator wlr_backend* () const { return m_backend; }

private:
    wlr_backend* m_backend;
};

#endif

#ifndef __LISTENER_HPP__
#define __LISTENER_HPP__

#include <functional>
#include <wayland-server-core.h>

template<typename T>
class Listener
{
public:
    Listener( wl_signal& signal, const std::function<void(T*)>& func )
    {
        // Ensure m_listener can be cast to Listener
        static_assert( offsetof( Listener, m_listener ) == 0 );

        m_func = func;
        m_listener.notify = []( wl_listener* listener, void* data ) {
            auto l = (Listener*)listener;
            l->m_func( (T*)data );
        };
        wl_signal_add( &signal, &m_listener );
    }

    ~Listener()
    {
        wl_list_remove( &m_listener.link );
    }

    Listener( const Listener& ) = delete;
    Listener( Listener&& ) = delete;
    Listener& operator=( const Listener& ) = delete;
    Listener& operator=( Listener&& ) = delete;

private:
    wl_listener m_listener;
    std::function<void(T*)> m_func;
};

#endif

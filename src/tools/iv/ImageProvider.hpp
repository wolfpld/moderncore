#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "util/TaskDispatch.hpp"

class Bitmap;

class ImageProvider
{
public:
    enum Result
    {
        Success,
        Error,
        Cancelled
    };

    typedef void(*Callback)( void* userData, int result, std::shared_ptr<Bitmap> bitmap );

    ImageProvider();
    ~ImageProvider();

    void LoadImage( const char* path, Callback callback, void* userData );
    void CancelRequest();

private:
    struct Job
    {
        std::string path;
        Callback callback;
        void* userData;
        bool cancel;
    };

    void Worker( std::shared_ptr<Job> job );

    TaskDispatch m_td;

    std::mutex m_lock;
    std::thread m_thread;
    std::shared_ptr<Job> m_job;
};

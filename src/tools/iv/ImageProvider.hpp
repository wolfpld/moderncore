#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include "util/TaskDispatch.hpp"

class Bitmap;
class DataBuffer;

class ImageProvider
{
public:
    enum Result
    {
        Success,
        Error,
        Cancelled
    };

    typedef void(*Callback)( void* userData, int64_t id, int result, std::shared_ptr<Bitmap> bitmap );

    ImageProvider();
    ~ImageProvider();

    int64_t LoadImage( const char* path, Callback callback, void* userData );
    int64_t LoadImage( std::unique_ptr<DataBuffer>&& buffer, Callback callback, void* userData );
    int64_t LoadImage( int fd, Callback callback, void* userData );

    void Cancel( int64_t id );
    void CancelAll();

private:
    struct Job
    {
        int64_t id;
        std::string path;
        std::unique_ptr<DataBuffer> buffer;
        int fd;
        Callback callback;
        void* userData;
    };

    void Worker();

    int64_t m_currentJob;
    int64_t m_nextId;
    std::vector<Job> m_jobs;

    std::atomic<bool> m_shutdown;
    std::mutex m_lock;
    std::condition_variable m_cv;
    std::thread m_thread;

    TaskDispatch m_td;
};

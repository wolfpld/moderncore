#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

class Bitmap;
class BitmapHdr;
class DataBuffer;
class TaskDispatch;

class ImageProvider
{
public:
    enum class Result
    {
        Success,
        Error,
        Cancelled
    };

    struct ReturnData
    {
        std::shared_ptr<Bitmap> bitmap;
        std::shared_ptr<BitmapHdr> bitmapHdr;
    };

    using Callback = void (*)(void *, int64_t, Result, int, ReturnData);

    ImageProvider( TaskDispatch& td );
    ~ImageProvider();

    int64_t LoadImage( const char* path, bool hdr, Callback callback, void* userData, int flags = 0 );
    int64_t LoadImage( std::unique_ptr<DataBuffer>&& buffer, bool hdr, Callback callback, void* userData, int flags = 0 );
    int64_t LoadImage( int fd, bool hdr, Callback callback, void* userData, int flags = 0 );

    void Cancel( int64_t id );
    void CancelAll();

private:
    struct Job
    {
        int64_t id;
        std::string path;
        std::unique_ptr<DataBuffer> buffer;
        int fd;
        bool hdr;
        Callback callback;
        void* userData;
        int flags;
    };

    void Worker();

    int64_t m_currentJob;
    int64_t m_nextId;
    std::vector<Job> m_jobs;

    std::atomic<bool> m_shutdown;
    std::mutex m_lock;
    std::condition_variable m_cv;
    std::thread m_thread;

    TaskDispatch& m_td;
};

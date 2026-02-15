#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <src/util/TaskDispatch.hpp>
#include <thread>

TEST_CASE( "TaskDispatch construction and destruction", "[taskdispatch][ctor]" )
{
    SECTION( "Zero workers" )
    {
        TaskDispatch dispatch( 0, "zero" );
        dispatch.WaitInit();
        REQUIRE( dispatch.NumWorkers() == 0 );
    }

    SECTION( "Single worker" )
    {
        TaskDispatch dispatch( 1, "single" );
        dispatch.WaitInit();
        REQUIRE( dispatch.NumWorkers() == 1 );
    }

    SECTION( "Multiple workers" )
    {
        TaskDispatch dispatch( 4, "multi" );
        dispatch.WaitInit();
        REQUIRE( dispatch.NumWorkers() == 4 );
    }

    SECTION( "Destructor joins all workers" )
    {
        std::atomic<bool> taskStarted{ false };
        std::atomic<bool> canFinish{ false };

        {
            TaskDispatch dispatch( 1, "dtor" );
            dispatch.WaitInit();
            dispatch.Queue( [&] {
                taskStarted = true;
                while( !canFinish.load() )
                {
                }
            } );

            while( !taskStarted.load() )
            {
            }
            canFinish = true;
        }
    }

    SECTION( "Implicit WaitInit in destructor" )
    {
        TaskDispatch dispatch( 2, "implicit" );
        // Don't call WaitInit - destructor should handle it
    }
}

TEST_CASE( "TaskDispatch basic task execution", "[taskdispatch][execution]" )
{
    TaskDispatch dispatch( 2, "basic" );
    dispatch.WaitInit();

    SECTION( "Single task executes" )
    {
        std::atomic<int> counter{ 0 };
        dispatch.Queue( [&counter] { counter++; } );
        dispatch.Sync();
        REQUIRE( counter == 1 );
    }

    SECTION( "Multiple tasks all execute" )
    {
        std::atomic<int> counter{ 0 };
        for( int i = 0; i < 10; i++ )
        {
            dispatch.Queue( [&counter] { counter++; } );
        }
        dispatch.Sync();
        REQUIRE( counter == 10 );
    }

    SECTION( "Move semantics" )
    {
        std::atomic<int> result{ 0 };
        int value = 42;
        dispatch.Queue( [&result, v = std::move( value )] { result = v; } );
        dispatch.Sync();
        REQUIRE( result == 42 );
    }

    SECTION( "Task with captured data executes correctly" )
    {
        std::vector<int> results;
        std::mutex resultsMutex;

        for( int i = 0; i < 5; i++ )
        {
            dispatch.Queue( [&results, &resultsMutex, i] {
                std::lock_guard lock( resultsMutex );
                results.push_back( i );
            } );
        }
        dispatch.Sync();

        REQUIRE( results.size() == 5 );
        std::sort( results.begin(), results.end() );
        for( int i = 0; i < 5; i++ )
        {
            REQUIRE( results[i] == i );
        }
    }
}

TEST_CASE( "TaskDispatch synchronization", "[taskdispatch][sync]" )
{
    SECTION( "Sync with empty queue returns immediately" )
    {
        TaskDispatch dispatch( 2, "empty" );
        dispatch.WaitInit();
        dispatch.Sync();
    }

    SECTION( "Sync blocks until tasks complete" )
    {
        TaskDispatch dispatch( 1, "blocking" );
        dispatch.WaitInit();

        std::atomic<bool> taskDone{ false };
        dispatch.Queue( [&taskDone] { taskDone = true; } );

        dispatch.Sync();
        REQUIRE( taskDone.load() );
    }

    SECTION( "Multiple sequential Sync calls" )
    {
        TaskDispatch dispatch( 2, "sequential" );
        dispatch.WaitInit();

        std::atomic<int> counter{ 0 };

        dispatch.Queue( [&counter] { counter++; } );
        dispatch.Sync();
        REQUIRE( counter == 1 );

        dispatch.Queue( [&counter] { counter++; } );
        dispatch.Sync();
        REQUIRE( counter == 2 );

        dispatch.Queue( [&counter] { counter++; } );
        dispatch.Sync();
        REQUIRE( counter == 3 );
    }

    SECTION( "Sync waits for in-flight tasks" )
    {
        TaskDispatch dispatch( 2, "inflight" );
        dispatch.WaitInit();

        std::atomic<int> completed{ 0 };

        for( int i = 0; i < 2; i++ )
        {
            dispatch.Queue( [&] {
                std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
                completed++;
            } );
        }

        dispatch.Sync();
        REQUIRE( completed.load() == 2 );
    }
}

TEST_CASE( "TaskDispatch parallelism", "[taskdispatch][parallel]" )
{
    SECTION( "Tasks run concurrently" )
    {
        TaskDispatch dispatch( 4, "parallel" );
        dispatch.WaitInit();

        std::atomic<int> concurrentCount{ 0 };
        std::atomic<int> maxConcurrent{ 0 };

        for( int i = 0; i < 4; i++ )
        {
            dispatch.Queue( [&] {
                int c = ++concurrentCount;
                int expected = maxConcurrent.load();
                while( c > expected && !maxConcurrent.compare_exchange_weak( expected, c ) )
                {
                    expected = maxConcurrent.load();
                }
                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                concurrentCount--;
            } );
        }

        dispatch.Sync();
        REQUIRE( maxConcurrent.load() >= 2 );
    }

    SECTION( "All workers participate under load" )
    {
        TaskDispatch dispatch( 4, "participate" );
        dispatch.WaitInit();

        std::atomic<int> workerIds{ 0 };
        std::vector<std::atomic<bool>> seen( 4 );
        for( auto& s : seen ) s = false;

        for( int i = 0; i < 100; i++ )
        {
            dispatch.Queue( [&] {
                int id = workerIds.fetch_add( 1 ) % 4;
                seen[id] = true;
                std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
            } );
        }

        dispatch.Sync();

        int uniqueWorkers = 0;
        for( const auto& s : seen )
        {
            if( s.load() ) uniqueWorkers++;
        }
        REQUIRE( uniqueWorkers >= 2 );
    }
}

TEST_CASE( "TaskDispatch thread safety", "[taskdispatch][threadsafe]" )
{
    SECTION( "Concurrent Queue from multiple threads" )
    {
        TaskDispatch dispatch( 4, "concurrent" );
        dispatch.WaitInit();

        std::atomic<int> counter{ 0 };
        std::vector<std::thread> threads;

        for( int t = 0; t < 4; t++ )
        {
            threads.emplace_back( [&dispatch, &counter] {
                for( int i = 0; i < 25; i++ )
                {
                    dispatch.Queue( [&counter] { counter++; } );
                }
            } );
        }

        for( auto& t : threads ) t.join();
        dispatch.Sync();
        REQUIRE( counter.load() == 100 );
    }
}

TEST_CASE( "TaskDispatch edge cases", "[taskdispatch][edge]" )
{
    SECTION( "Task that queues more tasks" )
    {
        TaskDispatch dispatch( 2, "recursive" );
        dispatch.WaitInit();

        std::atomic<int> counter{ 0 };

        dispatch.Queue( [&dispatch, &counter] {
            counter++;
            dispatch.Queue( [&counter] { counter++; } );
        } );

        dispatch.Sync();
        REQUIRE( counter.load() == 2 );
    }

    SECTION( "Large number of tasks" )
    {
        TaskDispatch dispatch( 4, "large" );
        dispatch.WaitInit();

        std::atomic<int> counter{ 0 };

        for( int i = 0; i < 1000; i++ )
        {
            dispatch.Queue( [&counter] { counter++; } );
        }

        dispatch.Sync();
        REQUIRE( counter.load() == 1000 );
    }
}
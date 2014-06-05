#pragma once

#include <sys/time.h>
#include <pthread.h>

#include <unordered_set>

inline timespec gettimeoffset ( long milliseconds )
{
    timeval tv;
    gettimeofday ( &tv, 0 );

    timespec ts;
    ts.tv_sec = tv.tv_sec + ( milliseconds / 1000L );
    ts.tv_nsec = tv.tv_usec * 1000L + ( milliseconds % 1000L ) * 1000000L;

    return ts;
}

class Thread
{
    volatile bool running;
    pthread_t thread;
    static void *func ( void *ptr );

public:

    inline Thread() : running ( false ) {}
    virtual ~Thread() { join(); }

    virtual void start()
    {
        if ( running )
            return;

        running = true;

        pthread_attr_t attr;
        pthread_attr_init ( &attr );
        pthread_create ( &thread, &attr, func, this );
        pthread_attr_destroy ( &attr );
    }

    virtual void join()
    {
        if ( !running )
            return;

        pthread_join ( thread, 0 );
        running = false;
    }

    inline void release() { running = false; }

    inline bool isRunning() const { return running; }

    virtual void run() = 0;
};

#define THREAD(NAME, CONTEXT)                                   \
    class NAME : public Thread                                  \
    {                                                           \
        CONTEXT& context;                                       \
    public:                                                     \
        NAME ( CONTEXT& context ) : context ( context ) {}      \
        void run();                                             \
    }

class Mutex
{
    pthread_mutex_t mutex;

public:

    inline Mutex()
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init ( &attr );
        pthread_mutexattr_settype ( &attr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init ( &mutex, &attr );
    }

    inline ~Mutex()
    {
        pthread_mutex_destroy ( &mutex );
    }

    inline void lock()
    {
        pthread_mutex_lock ( &mutex );
    }

    inline void unlock()
    {
        pthread_mutex_unlock ( &mutex );
    }

    friend class CondVar;
};

class Lock
{
    Mutex& mutex;

public:

    inline Lock ( Mutex& mutex ) : mutex ( mutex )
    {
        mutex.lock();
    }

    inline ~Lock()
    {
        mutex.unlock();
    }
};

#define LOCK(MUTEX) Lock lock ## MUTEX ( MUTEX )

class CondVar
{
    pthread_cond_t cond;

public:

    inline CondVar()
    {
        pthread_cond_init ( &cond, 0 );
    }

    inline ~CondVar()
    {
        pthread_cond_destroy ( &cond );
    }

    inline int wait ( Mutex& mutex )
    {
        return pthread_cond_wait ( &cond, & ( mutex.mutex ) );
    }

    inline int wait ( Mutex& mutex, long timeout )
    {
        timespec ts = gettimeoffset ( timeout );
        return pthread_cond_timedwait ( &cond, & ( mutex.mutex ), &ts );
    }

    inline void signal()
    {
        pthread_cond_signal ( &cond );
    }

    inline void broadcast()
    {
        pthread_cond_broadcast ( &cond );
    }
};

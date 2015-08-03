#pragma once

#include <sys/time.h>
#include <pthread.h>

#include <memory>


inline timespec gettimeoffset ( long milliseconds )
{
    timeval tv;
    gettimeofday ( &tv, 0 );

    timespec ts;
    ts.tv_sec = tv.tv_sec + ( milliseconds / 1000L );
    ts.tv_nsec = tv.tv_usec * 1000L + ( milliseconds % 1000L ) * 1000000L;

    return ts;
}


class Mutex
{
public:

    Mutex()
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init ( &attr );
        pthread_mutexattr_settype ( &attr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init ( &_mutex, &attr );
    }

    ~Mutex()
    {
        pthread_mutex_destroy ( &_mutex );
    }

    void lock()
    {
        pthread_mutex_lock ( &_mutex );
    }

    void unlock()
    {
        pthread_mutex_unlock ( &_mutex );
    }

    friend class CondVar;

private:

    pthread_mutex_t _mutex;
};


class Lock
{
public:

    Lock ( Mutex& mutex ) : _mutex ( mutex )
    {
        _mutex.lock();
    }

    ~Lock()
    {
        _mutex.unlock();
    }

private:

    Mutex& _mutex;
};


#define LOCK(MUTEX) Lock lock ## MUTEX ( MUTEX )


class CondVar
{
public:

    CondVar()
    {
        pthread_cond_init ( &cond, 0 );
    }

    ~CondVar()
    {
        pthread_cond_destroy ( &cond );
    }

    int wait ( Mutex& mutex )
    {
        return pthread_cond_wait ( &cond, & ( mutex._mutex ) );
    }

    int wait ( Mutex& mutex, long timeout )
    {
        timespec ts = gettimeoffset ( timeout );
        return pthread_cond_timedwait ( &cond, & ( mutex._mutex ), &ts );
    }

    void signal()
    {
        pthread_cond_signal ( &cond );
    }

    void broadcast()
    {
        pthread_cond_broadcast ( &cond );
    }

private:

    pthread_cond_t cond;
};


class Thread
{
public:

    virtual ~Thread() { join(); }

    virtual void start();

    virtual void join();

    void release();

    bool isRunning() const
    {
        LOCK ( _mutex );
        return _running;
    }

    virtual void run() = 0;

private:
    bool _running = false;

    pthread_t thread;

    mutable Mutex _mutex;

    static void *func ( void *ptr );
};


typedef std::shared_ptr<Thread> ThreadPtr;


#define THREAD(NAME, CONTEXT)                                   \
    class NAME : public Thread {                                \
        CONTEXT& context;                                       \
    public:                                                     \
        NAME ( CONTEXT& context ) : context ( context ) {}      \
        void run() override;                                    \
    } //

#pragma once

#include "Threading.h"

#include <list>
#include <unordered_set>

template<typename T> class BlockingQueue
{
    std::list<T> queue;
    mutable Mutex mutex;
    mutable CondVar cond;

public:
    void push ( const T& t )
    {
        LOCK ( mutex );

        queue.push_back ( t );
        cond.signal();
    }

    void push_front ( const T& t )
    {
        LOCK ( mutex );

        queue.push_front ( t );
        cond.signal();
    }

    T pop()
    {
        T t;
        LOCK ( mutex );

        while ( queue.empty() )
            cond.wait ( mutex );

        t = queue.front();
        queue.pop_front();

        return t;
    }

    T pop ( long timeout, T placebo )
    {
        int ret = 0;
        LOCK ( mutex );

        while ( !ret && queue.empty() )
            ret = cond.wait ( mutex, timeout );

        if ( !ret && !queue.empty() )
        {
            placebo = queue.front();
            queue.pop_front();
        }

        return placebo;
    }

    std::size_t size() const
    {
        LOCK ( mutex );
        return queue.size();
    }

    bool empty() const
    {
        LOCK ( mutex );
        return queue.empty();
    }

    void clear()
    {
        LOCK ( mutex );
        queue.clear();
    }
};

template<typename T> class BlockingSetQueue
{
    std::list<T> queue;
    std::tr1::unordered_set<T> set;
    mutable Mutex mutex;
    mutable CondVar cond;

public:
    bool push ( const T& t )
    {
        LOCK ( mutex );

        if ( set.find ( t ) == set.end() )
        {
            queue.push_back ( t );
            set.insert ( t );

            cond.signal();
            return true;
        }

        return false;
    }

    bool push_front ( const T& t )
    {
        LOCK ( mutex );

        if ( set.find ( t ) == set.end() )
        {
            queue.push_front ( t );
            set.insert ( t );

            cond.signal();
            return true;
        }

        return false;
    }

    T pop()
    {
        T t;
        LOCK ( mutex );

        while ( queue.empty() )
            cond.wait ( mutex );

        t = queue.front();
        queue.pop_front();
        set.erase ( t );

        return t;
    }

    T pop ( long timeout, T placebo )
    {
        int ret = 0;
        LOCK ( mutex );

        while ( !ret && queue.empty() )
            ret = cond.wait ( mutex, timeout );

        if ( !ret && !queue.empty() )
        {
            placebo = queue.front();
            queue.pop_front();
            set.erase ( placebo );
        }

        return placebo;
    }

    std::size_t size() const
    {
        LOCK ( mutex );
        return queue.size();
    }

    bool empty() const
    {
        LOCK ( mutex );
        return queue.empty();
    }

    void clear()
    {
        LOCK ( mutex );
        queue.clear();
    }
};

template<typename T, std::size_t N> class StaticBlockingQueue
{
    T elements[N];
    std::size_t count, head, tail;
    mutable Mutex mutex;
    mutable CondVar cond;

public:
    StaticBlockingQueue() : count ( 0 ), head ( 0 ), tail ( 0 ) {}

    void push ( const T& t )
    {
        LOCK ( mutex );

        while ( count == N )
            cond.wait ( mutex );

        elements[head++] = t;
        head %= N;
        ++count;

        cond.signal();
    }

    bool push ( const T& t, long timeout )
    {
        int ret = 0;
        LOCK ( mutex );

        while ( !ret && count == N )
            ret = cond.wait ( mutex, timeout );

        if ( !ret && count < N )
        {
            elements[head++] = t;
            head %= N;
            ++count;

            cond.signal();
            return true;
        }

        return false;
    }

    T pop()
    {
        T t;
        LOCK ( mutex );

        while ( count == 0 )
            cond.wait ( mutex );

        t = elements[tail++];
        tail %= N;
        --count;

        cond.signal();
        return t;
    }

    T pop ( long timeout, T placebo )
    {
        int ret = 0;
        LOCK ( mutex );

        while ( !ret && count == 0 )
            ret = cond.wait ( mutex, timeout );

        if ( !ret && count > 0 )
        {
            placebo = elements[tail++];
            tail %= N;
            --count;

            cond.signal();
        }

        return placebo;
    }

    std::size_t size() const
    {
        LOCK ( mutex );
        return count;
    }

    bool empty() const
    {
        LOCK ( mutex );
        return ( count == 0 );
    }

    void clear()
    {
        LOCK ( mutex );
        count = head = tail = 0;
    }
};

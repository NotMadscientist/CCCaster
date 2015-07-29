#pragma once

#include "Thread.hpp"

#include <list>
#include <unordered_set>


template<typename T> class BlockingQueue
{
public:

    void push ( const T& t )
    {
        LOCK ( _mutex );

        _queue.push_back ( t );
        _cond.signal();
    }

    void push_front ( const T& t )
    {
        LOCK ( _mutex );

        _queue.push_front ( t );
        _cond.signal();
    }

    T pop()
    {
        T t;
        LOCK ( _mutex );

        while ( _queue.empty() )
            _cond.wait ( _mutex );

        t = _queue.front();
        _queue.pop_front();

        return t;
    }

    T pop ( long timeout, T placeholder )
    {
        int ret = 0;
        LOCK ( _mutex );

        while ( !ret && _queue.empty() )
            ret = _cond.wait ( _mutex, timeout );

        if ( !ret && !_queue.empty() )
        {
            placeholder = _queue.front();
            _queue.pop_front();
        }

        return placeholder;
    }

    size_t size() const
    {
        LOCK ( _mutex );
        return _queue.size();
    }

    bool empty() const
    {
        LOCK ( _mutex );
        return _queue.empty();
    }

    void clear()
    {
        LOCK ( _mutex );
        _queue.clear();
    }

private:

    std::list<T> _queue;

    mutable Mutex _mutex;

    mutable CondVar _cond;
};


template<typename T> class BlockingSetQueue
{
public:

    bool push ( const T& t )
    {
        LOCK ( _mutex );

        if ( _set.find ( t ) == _set.end() )
        {
            _queue.push_back ( t );
            _set.insert ( t );

            _cond.signal();
            return true;
        }

        return false;
    }

    bool push_front ( const T& t )
    {
        LOCK ( _mutex );

        if ( _set.find ( t ) == _set.end() )
        {
            _queue.push_front ( t );
            _set.insert ( t );

            _cond.signal();
            return true;
        }

        return false;
    }

    T pop()
    {
        T t;
        LOCK ( _mutex );

        while ( _queue.empty() )
            _cond.wait ( _mutex );

        t = _queue.front();
        _queue.pop_front();
        _set.erase ( t );

        return t;
    }

    T pop ( long timeout, T placeholder )
    {
        int ret = 0;
        LOCK ( _mutex );

        while ( !ret && _queue.empty() )
            ret = _cond.wait ( _mutex, timeout );

        if ( !ret && !_queue.empty() )
        {
            placeholder = _queue.front();
            _queue.pop_front();
            _set.erase ( placeholder );
        }

        return placeholder;
    }

    size_t size() const
    {
        LOCK ( _mutex );
        return _queue.size();
    }

    bool empty() const
    {
        LOCK ( _mutex );
        return _queue.empty();
    }

    void clear()
    {
        LOCK ( _mutex );
        _queue.clear();
    }

private:

    std::list<T> _queue;

    std::unordered_set<T> _set;

    mutable Mutex _mutex;

    mutable CondVar _cond;
};


template<typename T, size_t N> class StaticBlockingQueue
{
public:

    void push ( const T& t )
    {
        LOCK ( _mutex );

        while ( _count == N )
            _cond.wait ( _mutex );

        _elements[_head++] = t;
        _head %= N;
        ++_count;

        _cond.signal();
    }

    bool push ( const T& t, long timeout )
    {
        int ret = 0;
        LOCK ( _mutex );

        while ( !ret && _count == N )
            ret = _cond.wait ( _mutex, timeout );

        if ( !ret && _count < N )
        {
            _elements[_head++] = t;
            _head %= N;
            ++_count;

            _cond.signal();
            return true;
        }

        return false;
    }

    T pop()
    {
        T t;
        LOCK ( _mutex );

        while ( _count == 0 )
            _cond.wait ( _mutex );

        t = _elements[_tail++];
        _tail %= N;
        --_count;

        _cond.signal();
        return t;
    }

    T pop ( long timeout, T placeholder )
    {
        int ret = 0;
        LOCK ( _mutex );

        while ( !ret && _count == 0 )
            ret = _cond.wait ( _mutex, timeout );

        if ( !ret && _count > 0 )
        {
            placeholder = _elements[_tail++];
            _tail %= N;
            --_count;

            _cond.signal();
        }

        return placeholder;
    }

    size_t size() const
    {
        LOCK ( _mutex );
        return _count;
    }

    bool empty() const
    {
        LOCK ( _mutex );
        return ( _count == 0 );
    }

    void clear()
    {
        LOCK ( _mutex );
        _count = _head = _tail = 0;
    }

private:

    T _elements[N];

    size_t _count = 0, _head = 0, _tail = 0;

    mutable Mutex _mutex;

    mutable CondVar _cond;
};

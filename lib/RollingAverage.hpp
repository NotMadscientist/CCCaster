#include "Logger.hpp"


template<typename T, size_t N>
class RollingAverage
{
public:

    RollingAverage() { reset(); }

    RollingAverage ( T initial ) { reset ( initial ); }

    void set ( T value )
    {
        _sum += value;

        if ( _count < N )
            ++_count;
        else
            _sum -= _values[_index];

        _average = _sum / _count;

        _values[_index] = value;

        _index = ( _index + 1 ) % N;
    }

    T get() const
    {
        return _average;
    }

    void reset()
    {
        _sum = _average = _index = _count = 0;
    }

    void reset ( T initial )
    {
        _sum = _average = initial;
        _index = _count = 1;
    }

    size_t count() const
    {
        return _count;
    }

    size_t size() const
    {
        return N;
    }

    bool full() const
    {
        return ( _count == N );
    }

private:

    T _values[N];

    T _sum, _average;

    size_t _index, _count;
};

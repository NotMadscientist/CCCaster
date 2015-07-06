#include "Logger.hpp"


template<typename T, size_t N>
class RollingAverage
{
    T values[N];

    T sum, average;

    size_t index, num;

public:

    RollingAverage() { reset(); }

    RollingAverage ( T initial ) { reset ( initial ); }

    void set ( T value )
    {
        sum += value;

        if ( num < N )
            ++num;
        else
            sum -= values[index];

        average = sum / num;

        values[index] = value;

        index = ( index + 1 ) % N;
    }

    T get() const
    {
        return average;
    }

    void reset()
    {
        sum = average = index = num = 0;
    }

    void reset ( T initial )
    {
        sum = average = initial;
        index = num = 1;
    }

    size_t count() const
    {
        return num;
    }

    size_t size() const
    {
        return N;
    }

    bool full() const
    {
        return ( num == N );
    }
};

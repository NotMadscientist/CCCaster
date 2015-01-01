#include "Logger.h"


template<typename T, size_t N>
class RollingAverage
{
    T values[N];
    T sum;
    size_t index, num;

public:

    RollingAverage() : sum ( 0 ), index ( 0 ), num ( 0 ) {}
    RollingAverage ( T initial ) : sum ( initial ), index ( 1 ), num ( 1 ) {}

    void set ( T value )
    {
        sum += value;

        if ( num < N )
            ++num;
        else
            sum -= values[index];

        values[index] = value;

        index = ( index + 1 ) % N;
    }

    T get() const
    {
        ASSERT ( num > 0 );

        return sum / num;
    }

    void reset()
    {
        sum = index = num = 0;
    }

    void reset ( T initial )
    {
        sum = initial;
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

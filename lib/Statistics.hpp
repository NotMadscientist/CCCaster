#pragma once

#include "Protocol.hpp"
#include "Logger.hpp"

#include <cmath>
#include <limits>
#include <algorithm>


// Template class to calculate stats with an online algorithm
class Statistics : public SerializableSequence
{
public:

    template<typename T>
    void addSample ( T value )
    {
        static_assert ( std::numeric_limits<double>::is_iec559, "IEEE 754 required" );

        if ( value > _worst )
            _worst = value;

        // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Incremental_algorithm
        double delta = value - _mean;
        ++_count;
        _mean += delta / _count;
        _sumOfSquaredDeltas += delta * ( value - _mean );
    }

    void reset()
    {
        _count = 0;
        _worst = -std::numeric_limits<double>::infinity();
        _mean = _sumOfSquaredDeltas = 0.0;
    }

    size_t getNumSamples() const
    {
        return _count;
    }

    double getWorst() const
    {
        return _worst;
    }

    double getMean() const
    {
        if ( _count < 1 )
            return 0;

        return _mean;
    }

    double getVariance() const
    {
        if ( _count < 2 )
            return 0;

        ASSERT ( _sumOfSquaredDeltas >= 0 );

        return _sumOfSquaredDeltas / ( _count - 1 );
    }

    double getStdDev() const
    {
        return std::sqrt ( getVariance() );
    }

    double getStdErr() const
    {
        if ( _count < 2 )
            return 0;

        return getStdDev() / std::sqrt ( _count );
    }

    void merge ( const Statistics& stats )
    {
        _worst = std::max ( _worst, stats._worst );
        _mean = ( _mean * _count + stats._mean * stats._count ) / ( _count + stats._count );
        _sumOfSquaredDeltas += stats._sumOfSquaredDeltas;
        _count += stats._count;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( Statistics, _count, _worst, _mean, _sumOfSquaredDeltas )

private:

    // Number of samples
    size_t _count = 0;

    double _worst = -std::numeric_limits<double>::infinity();

    // Current mean value
    double _mean = 0.0;

    // Sum of (latency - mean)^2 for each latency value
    double _sumOfSquaredDeltas = 0.0;
};


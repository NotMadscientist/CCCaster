#pragma once

#include <cmath>

#include "Protocol.h"


// Template class to calculate stats with an online algorithm
class Statistics : public SerializableSequence
{
    // Number of samples
    size_t count = 0;

    // Current mean value
    double mean = 0.0;

    // Sum of (latency - mean)^2 for each latency value
    double sumOfSquaredDeltas = 0.0;

public:

    template<typename T>
    void addValue ( T value )
    {
        // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Incremental_algorithm
        double delta = value - mean;
        ++count;
        mean += delta / count;
        sumOfSquaredDeltas += delta * ( value - mean );
    }

    void reset() { count = 0; mean = sumOfSquaredDeltas = 0.0; }

    size_t getNumSamples() const { return count; }

    double getMean() const
    {
        if ( count < 1 )
            return -1;

        return mean;
    }

    double getJitter() const
    {
        if ( count < 2 )
            return -1;

        return std::sqrt ( sumOfSquaredDeltas / ( count - 1 ) );
    }

    void merge ( const Statistics& stats )
    {
        mean = ( mean * count + stats.mean * stats.count ) / ( count + stats.count );
        sumOfSquaredDeltas += stats.sumOfSquaredDeltas;
        count += stats.count;
    }

    PROTOCOL_BOILERPLATE ( count, mean, sumOfSquaredDeltas );
};


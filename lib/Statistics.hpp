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

        if ( value > worst )
            worst = value;

        // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Incremental_algorithm
        double delta = value - mean;
        ++count;
        mean += delta / count;
        sumOfSquaredDeltas += delta * ( value - mean );
    }

    void reset()
    {
        count = 0;
        worst = -std::numeric_limits<double>::infinity();
        mean = sumOfSquaredDeltas = 0.0;
    }

    size_t getNumSamples() const
    {
        return count;
    }

    double getWorst() const
    {
        return worst;
    }

    double getMean() const
    {
        if ( count < 1 )
            return 0;

        return mean;
    }

    double getVariance() const
    {
        if ( count < 2 )
            return 0;

        ASSERT ( sumOfSquaredDeltas >= 0 );

        return sumOfSquaredDeltas / ( count - 1 );
    }

    double getStdDev() const
    {
        return std::sqrt ( getVariance() );
    }

    double getStdErr() const
    {
        if ( count < 2 )
            return 0;

        return getStdDev() / std::sqrt ( count );
    }

    void merge ( const Statistics& stats )
    {
        worst = std::max ( worst, stats.worst );
        mean = ( mean * count + stats.mean * stats.count ) / ( count + stats.count );
        sumOfSquaredDeltas += stats.sumOfSquaredDeltas;
        count += stats.count;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( Statistics, count, worst, mean, sumOfSquaredDeltas )

private:

    // Number of samples
    size_t count = 0;

    double worst = -std::numeric_limits<double>::infinity();

    // Current mean value
    double mean = 0.0;

    // Sum of (latency - mean)^2 for each latency value
    double sumOfSquaredDeltas = 0.0;
};


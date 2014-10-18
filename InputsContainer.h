#pragma once

#include "Constants.h"
#include "Logger.h"

#include <vector>
#include <algorithm>


template<typename T>
class InputsContainer
{
    // Mapping: index -> frame -> input
    std::vector<std::vector<T>> inputs;

    // Mapping: index -> frame -> boolean
    std::vector<std::vector<bool>> real;

    // Last frame of input that changed
    IndexedFrame lastChangedFrame = { { UINT_MAX, UINT_MAX } };

public:

    bool fillFakeInputs = false;

    T get ( IndexedFrame frame ) const
    {
        if ( frame.parts.index >= inputs.size() )
            return 0;

        if ( inputs[frame.parts.index].empty() )
            return 0;

        if ( frame.parts.frame >= inputs[frame.parts.index].size() )
            return inputs[frame.parts.index].back();

        return inputs[frame.parts.index][frame.parts.frame];
    }

    void get ( IndexedFrame frame, T *t, size_t n ) const
    {
        ASSERT ( frame.parts.index < inputs.size() );
        ASSERT ( frame.parts.frame + n <= inputs[frame.parts.index].size() );

        std::copy ( inputs[frame.parts.index].begin() + frame.parts.frame,
                    inputs[frame.parts.index].begin() + frame.parts.frame + n, t );
    }

    void set ( IndexedFrame frame, T t )
    {
        resize ( frame );

        inputs[frame.parts.index][frame.parts.frame] = t;
    }

    void set ( IndexedFrame frame, T t, size_t n )
    {
        resize ( frame, n, true );

        std::fill ( inputs[frame.parts.index].begin() + frame.parts.frame,
                    inputs[frame.parts.index].begin() + frame.parts.frame + n, t );
    }

    void set ( IndexedFrame frame, const T *t, size_t n )
    {
        // TODO refill inputs when faked inputs change

        IndexedFrame f;
        size_t i;

        for ( i = 0, f = frame; i < n; ++i, ++f.parts.frame )
        {
            if ( get ( f ) == t[i] )
                continue;

            // Indicate changed if the input is different from the last known input
            lastChangedFrame.value = std::min ( lastChangedFrame.value, f.value );
            break;
        }

        resize ( frame, n, true );

        std::copy ( t, t + n, &inputs[frame.parts.index][frame.parts.frame] );
    }

    void resize ( IndexedFrame frame, size_t n = 1, bool isReal = true )
    {
        if ( frame.parts.index >= inputs.size() )
            inputs.resize ( frame.parts.index + 1 );

        if ( frame.parts.frame + n > inputs[frame.parts.index].size() )
            inputs[frame.parts.index].resize ( frame.parts.frame + n, 0 );

        if ( fillFakeInputs )
        {
            if ( frame.parts.index >= real.size() )
                real.resize ( frame.parts.index + 1 );

            if ( frame.parts.frame + n > real[frame.parts.index].size() )
                real[frame.parts.index].resize ( frame.parts.frame + n, false );

            if ( isReal )
            {
                std::fill ( real[frame.parts.index].begin() + frame.parts.frame,
                            real[frame.parts.index].begin() + frame.parts.frame + n, true );
            }
        }
    }

    void clear ( size_t startIndex, size_t endIndex )
    {
        for ( size_t i = startIndex; i < endIndex; ++i )
        {
            if ( i >= inputs.size() )
                break;

            inputs[i].clear();
            inputs[i].shrink_to_fit();
        }

        for ( size_t i = startIndex; i < endIndex; ++i )
        {
            if ( i >= real.size() )
                break;

            real[i].clear();
            real[i].shrink_to_fit();
        }
    }

    bool empty() const { return inputs.empty(); }

    bool empty ( size_t index ) const
    {
        if ( index + 1 > inputs.size() )
            return false;

        return inputs[index].empty();
    }

    uint32_t getEndIndex() const { return inputs.size(); }

    uint32_t getEndFrame() const
    {
        if ( inputs.empty() )
            return 0;

        return inputs.back().size();
    }

    IndexedFrame getEndIndexedFrame() const { return { getEndIndex(), getEndFrame() }; }

    const IndexedFrame& getLastChangedFrame() const { return lastChangedFrame; }

    void clearLastChangedFrame() { lastChangedFrame = { { UINT_MAX, UINT_MAX } }; }
};

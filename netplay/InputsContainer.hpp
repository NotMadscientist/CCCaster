#pragma once

#include "Constants.hpp"
#include "Logger.hpp"

#include <vector>
#include <algorithm>


template<typename T>
class InputsContainer
{
public:

    // Get a single input for the given index:frame, returns 0 if none.
    T get ( uint32_t index, uint32_t frame ) const
    {
        if ( index >= _inputs.size() || _inputs[index].empty() )
            return lastInputBefore ( index );

        if ( frame >= _inputs[index].size() )
            return _inputs[index].back();

        return _inputs[index][frame];
    }

    // Get n inputs starting from the given index:frame, ASSERTS if not enough.
    void get ( uint32_t index, uint32_t frame, T *t, size_t n ) const
    {
        ASSERT ( index < _inputs.size() );
        ASSERT ( frame + n <= _inputs[index].size() );

        std::copy ( _inputs[index].begin() + frame,
                    _inputs[index].begin() + frame + n, t );
    }

    // Set a single input for the given index:frame, CANNOT change existing inputs.
    void set ( uint32_t index, uint32_t frame, T t )
    {
        if ( _inputs.size() > index && _inputs[index].size() > frame )
            return;

        resize ( index, frame );

        _inputs[index][frame] = t;
    }

    // Assign a single input for the given index:frame, CAN change existing inputs
    void assign ( uint32_t index, uint32_t frame, T t )
    {
        resize ( index, frame );

        _inputs[index][frame] = t;
    }

    // Fill n inputs with the same given value starting from the given index:frame, CAN change existing inputs.
    void set ( uint32_t index, uint32_t frame, T t, size_t n )
    {
        resize ( index, frame, n );

        std::fill ( _inputs[index].begin() + frame,
                    _inputs[index].begin() + frame + n, t );
    }

    // Set n inputs starting from the given index:frame, CAN change existing inputs.
    void set ( uint32_t index, uint32_t frame, const T *t, size_t n, uint32_t checkStartingFromIndex = UINT_MAX )
    {
        if ( index >= checkStartingFromIndex )
        {
            IndexedFrame f;
            size_t i;

            for ( i = 0, f = {{ frame, index }}; i < n; ++i, ++f.parts.frame )
            {
                if ( get ( f.parts.index, f.parts.frame ) == t[i] )
                    continue;

                // Indicate changed if the input is different from the last known input
                _lastChangedFrame.value = std::min ( _lastChangedFrame.value, f.value );
                break;
            }
        }

        resize ( index, frame, n );

        std::copy ( t, t + n, &_inputs[index][frame] );
    }

    // Resize the container so that it can contain inputs up to index:frame+n.
    void resize ( uint32_t index, uint32_t frame, size_t n = 1 )
    {
        T last = 0;

        if ( index >= _inputs.size() )
        {
            last = lastInputBefore ( _inputs.size() );
            _inputs.resize ( index + 1 );
        }
        else if ( ! _inputs[index].empty() )
        {
            last = _inputs[index].back();
        }

        if ( frame + n > _inputs[index].size() )
            _inputs[index].resize ( frame + n, last );
    }

    void clear()
    {
        _inputs.clear();
    }

    bool empty() const
    {
        return _inputs.empty();
    }

    bool empty ( size_t index ) const
    {
        if ( index >= _inputs.size() )
            return true;

        return _inputs[index].empty();
    }

    uint32_t getEndIndex() const
    {
        return _inputs.size();
    }

    uint32_t getEndFrame() const
    {
        if ( _inputs.empty() )
            return 0;

        return _inputs.back().size();
    }

    uint32_t getEndFrame ( size_t index ) const
    {
        if ( index >= _inputs.size() )
            return 0;

        return _inputs[index].size();
    }

    void eraseIndexOlderThan ( size_t index )
    {
        if ( index + 1 >= _inputs.size() )
            _inputs.clear();
        else
            _inputs.erase ( _inputs.begin(), _inputs.begin() + index );
    }

    IndexedFrame getLastChangedFrame() const
    {
        return _lastChangedFrame;
    }

    void clearLastChangedFrame()
    {
        _lastChangedFrame = MaxIndexedFrame;
    }

private:

    // Mapping: index -> frame -> input
    std::vector<std::vector<T>> _inputs;

    // Last frame of input that changed
    IndexedFrame _lastChangedFrame = MaxIndexedFrame;

    // Get the last known input BEFORE the given index. Defaults to 0 if unknown.
    T lastInputBefore ( uint32_t index ) const
    {
        if ( _inputs.empty() || index == 0 )
            return 0;

        if ( index > _inputs.size() )
            index = _inputs.size();

        do
        {
            --index;
            if ( ! _inputs[index].empty() )
                return _inputs[index].back();
        }
        while ( index > 0 );

        return 0;
    }
};

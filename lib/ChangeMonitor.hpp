#pragma once

#include <vector>
#include <memory>


class ChangeMonitor
{
public:

    struct Interface
    {
        // To be implemented by derived types, should return true if changed
        virtual bool check() = 0;
    };

    // Add a RefChangeMonitor, returns a pointer that can be used to remove the monitor
    template<typename O, typename K, typename T>
    ChangeMonitor::Interface *addRef ( O *owner, K key, const T& ref );

    // Add a PtrToRefChangeMonitor, returns a pointer that can be used to remove the monitor
    template<typename O, typename K, typename T>
    ChangeMonitor::Interface *addPtrToRef ( O *owner, K key, const T *&ptr, T nullPtrValue );

    // Remove a ChangeMonitor::Interface, returns true only if something was removed
    bool remove ( ChangeMonitor::Interface *monitor )
    {
        for ( auto it = _monitors.begin(); it != _monitors.end(); ++it )
        {
            if ( it->get() == monitor )
            {
                _monitors.erase ( it );
                return true;
            }
        }

        return false;
    }

    // Check all monitors for changes
    void check()
    {
        // Iterate using indices, because the list can change
        for ( size_t i = 0; i < _monitors.size(); ++i )
            _monitors[i]->check();
    }

    // Clear all monitors
    void clear()
    {
        _monitors.clear();
    }

    // Get the singleton instance
    static ChangeMonitor& get();

private:

    // List of all the ChangeMonitor::Interface implementations
    std::vector<std::shared_ptr<ChangeMonitor::Interface>> _monitors;

    // Private constructor, etc. for singleton class
    ChangeMonitor();
    ChangeMonitor ( const ChangeMonitor& );
    const ChangeMonitor& operator= ( const ChangeMonitor& );
};

typedef std::shared_ptr<ChangeMonitor::Interface> ChangeMonitorPtr;


// This monitors for changes to a reference
template<typename K, typename T>
class RefChangeMonitor : public ChangeMonitor::Interface
{
public:

    struct Owner
    {
        virtual void changedValue ( K key, T previous, T current ) = 0;
    };

    Owner *owner = 0;

    RefChangeMonitor ( Owner *owner, K key, const T& ref )
        : owner ( owner )
        , _key ( key )
        , _current ( ref )
        , _previous ( ref ) {}

    bool check() override
    {
        if ( _current == _previous )
            return false;

        if ( owner )
            owner->changedValue ( _key, _previous, _current );

        _previous = _current;
        return true;
    }

private:

    // The key that identifies the callback
    const K _key;

    // The reference of the current value
    const T& _current;

    // The previous value, also initial value
    T _previous;
};

template<typename O, typename K, typename T>
ChangeMonitor::Interface *ChangeMonitor::addRef ( O *owner, K key, const T& ref )
{
    typedef typename RefChangeMonitor<K, T>::Owner Owner;

    ChangeMonitorPtr monitor ( new RefChangeMonitor<K, T> ( static_cast<Owner *> ( owner ), key, ref ) );

    _monitors.push_back ( monitor );

    return monitor.get();
}


// This monitors for changes of a pointer to a reference (the reference not the pointer)
template<typename K, typename T>
class PtrToRefChangeMonitor : public ChangeMonitor::Interface
{
public:

    struct Owner
    {
        virtual void changedValue ( K key, T previous, T current ) = 0;
    };

    Owner *owner = 0;

    PtrToRefChangeMonitor ( Owner *owner, K key, const T *&ptr, T nullPtrValue )
        : owner ( owner )
        , _key ( key )
        , _ptr ( ptr )
        , _previous ( nullPtrValue )
        , _nullPtrValue ( nullPtrValue )
    {
        if ( _ptr )
            _previous = *_ptr;
    }

    bool check() override
    {
        T current = _nullPtrValue;

        if ( _ptr )
            current = *_ptr;

        if ( current == _previous )
            return false;

        if ( owner )
            owner->changedValue ( _key, _previous, current );

        _previous = current;
        return true;
    }

private:

    // The key that identifies the callback
    const K _key;

    // The pointer to the reference of the current of value
    const T *&_ptr;

    // The previous value, also initial value
    T _previous;

    // The value to use if the pointer is null
    const T _nullPtrValue;
};

template<typename O, typename K, typename T>
ChangeMonitor::Interface *ChangeMonitor::addPtrToRef ( O *owner, K key, const T *&ptr, T nullPtrValue )
{
    typedef typename PtrToRefChangeMonitor<K, T>::Owner Owner;

    ChangeMonitorPtr monitor (
        new PtrToRefChangeMonitor<K, T> ( static_cast<Owner *> ( owner ), key, ptr, nullPtrValue ) );

    _monitors.push_back ( monitor );

    return monitor.get();
}

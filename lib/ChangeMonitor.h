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

private:

    // List of all the ChangeMonitor::Interface implmentations
    std::vector<std::shared_ptr<ChangeMonitor::Interface>> monitors;

    // Private constructor, etc. for singleton class
    ChangeMonitor();
    ChangeMonitor ( const ChangeMonitor& );
    const ChangeMonitor& operator= ( const ChangeMonitor& );

public:

    // Add a RefChangeMonitor, returns a pointer that can be used to remove the monitor
    template<typename O, typename K, typename T>
    ChangeMonitor::Interface *addRef ( O *owner, K key, const T& ref );

    // Add a PtrToRefChangeMonitor, returns a pointer that can be used to remove the monitor
    template<typename O, typename K, typename T>
    ChangeMonitor::Interface *addPtrToRef ( O *owner, K key, const T *&ptr, T nullPtrValue );

    // Remove a ChangeMonitor::Interface, returns true only if something was removed
    bool remove ( ChangeMonitor::Interface *monitor )
    {
        for ( auto it = monitors.begin(); it != monitors.end(); ++it )
        {
            if ( it->get() == monitor )
            {
                monitors.erase ( it );
                return true;
            }
        }

        return false;
    }

    // Check all monitors for changes
    void check()
    {
        // Iterate using indicies, because the list can change
        for ( size_t i = 0; i < monitors.size(); ++i )
            monitors[i]->check();
    }

    // Clear all monitors
    void clear()
    {
        monitors.clear();
    }

    // Get the singleton instance
    static ChangeMonitor& get();
};

typedef std::shared_ptr<ChangeMonitor::Interface> ChangeMonitorPtr;


// This monitors for changes to a reference
template<typename K, typename T>
class RefChangeMonitor : public ChangeMonitor::Interface
{
public:

    struct Owner
    {
        virtual void hasChanged ( const K& key, T previous, T current ) {}
    };

    Owner *owner = 0;

private:

    // The key that identifies the callback
    const K key;

    // The reference of the current value
    const T& current;

    // The previous value, also initial value
    T previous;

public:

    RefChangeMonitor ( Owner *owner, K key, const T& ref )
        : owner ( owner )
        , key ( key )
        , current ( ref )
        , previous ( ref ) {}

    bool check() override
    {
        if ( current == previous )
            return false;

        if ( owner )
            owner->hasChanged ( key, previous, current );

        previous = current;
        return true;
    }
};

template<typename O, typename K, typename T>
ChangeMonitor::Interface *ChangeMonitor::addRef ( O *owner, K key, const T& ref )
{
    typedef typename RefChangeMonitor<K, T>::Owner Owner;

    ChangeMonitorPtr monitor ( new RefChangeMonitor<K, T> ( static_cast<Owner *> ( owner ), key, ref ) );

    monitors.push_back ( monitor );

    return monitor.get();
}


// This monitors for changes of a pointer to a reference (the reference not the pointer)
template<typename K, typename T>
class PtrToRefChangeMonitor : public ChangeMonitor::Interface
{
public:

    struct Owner
    {
        virtual void hasChanged ( const K& key, T previous, T current ) {}
    };

    Owner *owner = 0;

private:

    // The key that identifies the callback
    const K key;

    // The pointer to the reference of the current of value
    const T *&ptr;

    // The previous value, also initial value
    T previous;

    // The value to use if the pointer is null
    const T nullPtrValue;

public:

    PtrToRefChangeMonitor ( Owner *owner, K key, const T *&ptr, T nullPtrValue )
        : owner ( owner )
        , key ( key )
        , ptr ( ptr )
        , previous ( nullPtrValue )
        , nullPtrValue ( nullPtrValue )
    {
        if ( ptr )
            previous = *ptr;
    }

    bool check() override
    {
        T current = nullPtrValue;

        if ( ptr )
            current = *ptr;

        if ( current == previous )
            return false;

        if ( owner )
            owner->hasChanged ( key, previous, current );

        previous = current;
        return true;
    }
};

template<typename O, typename K, typename T>
ChangeMonitor::Interface *ChangeMonitor::addPtrToRef ( O *owner, K key, const T *&ptr, T nullPtrValue )
{
    typedef typename PtrToRefChangeMonitor<K, T>::Owner Owner;

    ChangeMonitorPtr monitor (
        new PtrToRefChangeMonitor<K, T> ( static_cast<Owner *> ( owner ), key, ptr, nullPtrValue ) );

    monitors.push_back ( monitor );

    return monitor.get();
}

#pragma once

#include <vector>
#include <memory>


class ChangeMonitor;
typedef std::shared_ptr<ChangeMonitor> ChangeMonitorPtr;


class ChangeMonitor
{
    // Private copy constructor, etc. for singleton class
    ChangeMonitor ( const ChangeMonitor& );
    const ChangeMonitor& operator= ( const ChangeMonitor& );

protected:

    // Protected constructor because this class is also an interface
    inline ChangeMonitor() {};

    // Empty base checkChanged method, to be implemented by derived types
    inline virtual void checkChanged() {};

    // List of all the ChangeMonitor implmentations
    std::vector<ChangeMonitorPtr> monitors;

public:

    // Add a RefChangeMonitor, returns a pointer that can be used to remove the monitor
    template<typename O, typename K, typename T>
    ChangeMonitor *addRef ( O *owner, K key, const T& ref );

    // Add a PtrToRefChangeMonitor, returns a pointer that can be used to remove the monitor
    template<typename O, typename K, typename T>
    ChangeMonitor *addPtrToRef ( O *owner, K key, const T *&ptr, T nullPtrValue );

    // Remove a ChangeMonitor, returns true only if something was removed
    inline bool remove ( ChangeMonitor *monitor )
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
    inline void check()
    {
        // Iterate using indicies, because the list can change
        for ( size_t i = 0; i < monitors.size(); ++i )
            monitors[i]->checkChanged();
    }

    // Clear all monitors
    inline void clear()
    {
        monitors.clear();
    }

    // Get the singleton instance
    static ChangeMonitor& get();
};


// This monitors for changes to a reference
template<typename K, typename T>
class RefChangeMonitor : public ChangeMonitor
{
public:

    struct Owner
    {
        inline virtual void hasChanged ( const K& key, T previous, T current ) {}
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

    inline RefChangeMonitor ( Owner *owner, K key, const T& ref )
        : owner ( owner )
        , key ( key )
        , current ( ref )
        , previous ( ref ) {}

protected:

    void checkChanged() override
    {
        if ( current == previous )
            return;

        if ( owner )
            owner->hasChanged ( key, previous, current );

        previous = current;
    }
};

template<typename O, typename K, typename T>
inline ChangeMonitor *ChangeMonitor::addRef ( O *owner, K key, const T& ref )
{
    typedef typename RefChangeMonitor<K, T>::Owner Owner;

    ChangeMonitorPtr monitor ( new RefChangeMonitor<K, T> ( static_cast<Owner *> ( owner ), key, ref ) );

    monitors.push_back ( monitor );

    return monitor.get();
}


// This monitors for changes of a pointer to a reference (the reference not the pointer)
template<typename K, typename T>
class PtrToRefChangeMonitor : public ChangeMonitor
{
public:

    struct Owner
    {
        inline virtual void hasChanged ( const K& key, T previous, T current ) {}
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

    inline PtrToRefChangeMonitor ( Owner *owner, K key, const T *&ptr, T nullPtrValue )
        : owner ( owner )
        , key ( key )
        , ptr ( ptr )
        , previous ( nullPtrValue )
        , nullPtrValue ( nullPtrValue )
    {
        if ( ptr )
            previous = *ptr;
    }

protected:

    void checkChanged() override
    {
        T current = nullPtrValue;

        if ( ptr )
            current = *ptr;

        if ( current == previous )
            return;

        if ( owner )
            owner->hasChanged ( key, previous, current );

        previous = current;
    }
};

template<typename O, typename K, typename T>
inline ChangeMonitor *ChangeMonitor::addPtrToRef ( O *owner, K key, const T *&ptr, T nullPtrValue )
{
    typedef typename PtrToRefChangeMonitor<K, T>::Owner Owner;

    ChangeMonitorPtr monitor (
        new PtrToRefChangeMonitor<K, T> ( static_cast<Owner *> ( owner ), key, ptr, nullPtrValue ) );

    monitors.push_back ( monitor );

    return monitor.get();
}

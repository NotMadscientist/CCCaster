#include "Event.h"

using namespace std;

void EventManager::addThread ( const shared_ptr<Thread>& thread )
{
    reaperThread.start();
    zombieThreads.push ( thread );
}

void EventManager::ReaperThread::run()
{
    for ( ;; )
    {
        shared_ptr<Thread> thread = zombieThreads.pop();

        if ( thread )
            thread->join();
        else
            return;
    }
}

void EventManager::ReaperThread::join()
{
    zombieThreads.push ( shared_ptr<Thread>() );
    Thread::join();
}

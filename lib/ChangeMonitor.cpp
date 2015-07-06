#include "ChangeMonitor.hpp"

using namespace std;


ChangeMonitor::ChangeMonitor() {}

ChangeMonitor& ChangeMonitor::get()
{
    static ChangeMonitor instance;
    return instance;
}

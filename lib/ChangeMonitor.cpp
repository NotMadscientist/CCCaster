#include "ChangeMonitor.h"

using namespace std;


ChangeMonitor::ChangeMonitor() {}

ChangeMonitor& ChangeMonitor::get()
{
    static ChangeMonitor cm;
    return cm;
}

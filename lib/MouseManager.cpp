#include "MouseManager.h"

using namespace std;


MouseManager& MouseManager::get()
{
    static MouseManager instance;
    return instance;
}

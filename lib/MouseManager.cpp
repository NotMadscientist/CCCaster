#include "MouseManager.hpp"

using namespace std;


MouseManager& MouseManager::get()
{
    static MouseManager instance;
    return instance;
}

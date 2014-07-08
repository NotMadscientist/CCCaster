#include "Log.h"
#include "Event.h"
#include "Test.h"

#include <windows.h>

#define LOG_FILE FOLDER "dll.log"

using namespace std;

extern "C" BOOL APIENTRY DllMain ( HMODULE, DWORD reason, LPVOID )
{
    switch ( reason )
    {
        case DLL_PROCESS_ATTACH:
            Log::get().initialize ( LOG_FILE );
            EventManager::get().initialize();
            LOG ( "DLL_PROCESS_ATTACH" );
            break;

        case DLL_PROCESS_DETACH:
            LOG ( "DLL_PROCESS_DETACH" );
            EventManager::get().deinitialize();
            Log::get().deinitialize();
            break;
    }

    return TRUE;
}

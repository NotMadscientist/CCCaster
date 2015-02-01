Hotkeys:

    F4 opens the overlay menu for setting and changing controllers.

    Ctrl + Number changes the delay.

    Alt + Number changes the rollback.

    Spacebar toggles fast-forward when spectating.



How to host without port forward:

    Host on any port normally (you can use port 0 to pick any available port).

    Then the client connects to the same provided IP-address:port.

    Relaying is automatically performed, however it is not guaranteed to work for all networks.

    Some routers may be too restrictive, in which case you must open a port to netplay.


Other notes:

    Run Add_Handler_Protocol.bat once as admin to launch directly from http://seemeinmelty.meteor.com/

    If you use an analog controller, make sure to adjust the deadzone, else you might get weird inputs.


Running under Wine:

    First you need to create a 32 bit wine prefix:

        rm -rf ~/.wine

        WINEARCH=win32 WINEPREFIX=~/.wine winecfg

    Also you need install native D3DX9 dlls using winetricks:

        wget http://kegel.com/wine/winetricks

        sh winetricks d3dx9

    Caveats:

        No in-game button config unless someone can figure out how to hook DirectX9 under Wine.

        Can't save replays because MBAA.exe crashes when saving replays under Wine.

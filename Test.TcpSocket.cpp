#include "Test.Socket.h"
#include "TcpSocket.h"

TEST_CONNECT                ( TcpSocket, 0, 0, 1000 )

TEST_TIMEOUT                ( TcpSocket, 0, 0, 1000 )

TEST_DISCONNECT_CLIENT      ( TcpSocket, 0, 0, 1000 )

TEST_DISCONNECT_ACCEPTED    ( TcpSocket, 0, 0, 1000 )

TEST_SEND                   ( TcpSocket, 0, 0, 1000 )

TEST_SEND_WITHOUT_SERVER    ( TcpSocket, 0, 0, 1000 )

TEST_SEND_PARTIAL           ( TcpSocket )

// #include "Test.Socket.h"
//
// #define PACKET_LOSS     90
// #define KEEP_ALIVE      ( 30 * 1000 )
//
// TEST_CONNECT                ( UdpSocket, Udp, PACKET_LOSS, KEEP_ALIVE, 60 * 1000 )
//
// TEST_TIMEOUT                ( UdpSocket, Udp, 0, 1000, 1000 )
//
// TEST_CLIENT_DISCONNECT      ( UdpSocket, Udp, 0, 1000, 3000 )
//
// TEST_SERVER_DISCONNECT      ( UdpSocket, Udp, 0, 1000, 5000 )
//
// TEST_SEND                   ( UdpSocket, Udp, PACKET_LOSS, KEEP_ALIVE, 120 * 1000 )
//
// // This test doesn't make sense since there is only one UDP socket
// // TEST_SEND_WITHOUT_SERVER    ( UdpSocket, Udp, PACKET_LOSS, KEEP_ALIVE, 120 * 1000 )

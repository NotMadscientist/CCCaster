#include "Test.Socket.h"

#define PACKET_LOSS     90
#define KEEP_ALIVE      ( 30 * 1000 )

TEST_CONNECT                ( ReliableUdp, Udp, PACKET_LOSS, KEEP_ALIVE, 60 * 1000 )

TEST_TIMEOUT                ( ReliableUdp, Udp, PACKET_LOSS, KEEP_ALIVE, 1000 )

TEST_CLIENT_DISCONNECT      ( ReliableUdp, Udp, 0, 0, 2000 )

TEST_SERVER_DISCONNECT      ( ReliableUdp, Udp, 0, 0, 2000 )

TEST_SEND                   ( ReliableUdp, Udp, PACKET_LOSS, KEEP_ALIVE, 120 * 1000 )

// This test doesn't make sense since there is only one UDP socket
// TEST_SEND_WITHOUT_SERVER    ( ReliableUdp, Udp, PACKET_LOSS, KEEP_ALIVE, 120 * 1000 )

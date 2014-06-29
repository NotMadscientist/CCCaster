#include "Test.Socket.h"

#define PACKET_LOSS     90
#define KEEP_ALIVE      ( 30 * 1000 )

TEST_CONNECT ( ReliableUdp, Udp, PACKET_LOSS, KEEP_ALIVE, 60 * 1000 )

TEST_TIMEOUT ( ReliableUdp, Udp, PACKET_LOSS, KEEP_ALIVE, 1000 )

TEST_SEND    ( ReliableUdp, Udp, PACKET_LOSS, KEEP_ALIVE, 120 * 1000 )

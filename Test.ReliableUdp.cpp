#include "Test.Socket.h"

#define PACKET_LOSS 90

TEST_CONNECT ( ReliableUdp, Udp, PACKET_LOSS, 60 * 1000 )

TEST_TIMEOUT ( ReliableUdp, Udp, PACKET_LOSS, 1000 )

TEST_SEND ( ReliableUdp, Udp, PACKET_LOSS, 120 * 1000 )

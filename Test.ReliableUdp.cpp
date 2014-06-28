#include "Test.Socket.h"

#define PACKET_LOSS 0

TEST_CONNECT ( ReliableUdp, Udp, PACKET_LOSS )

TEST_TIMEOUT ( ReliableUdp, Udp, PACKET_LOSS )

TEST_SEND ( ReliableUdp, Udp, PACKET_LOSS )

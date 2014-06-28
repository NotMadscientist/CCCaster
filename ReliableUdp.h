#include "Socket.h"
#include "GoBackN.h"

class ReliableUdp : public GoBackN::Owner, public Socket
{
    class ProxyOwner : public GoBackN::Owner, public Socket::Owner
    {
        Socket::Owner *owner;

    public:

        inline ProxyOwner ( Socket::Owner *owner ) : owner ( owner ) {}

        void sendGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
        void recvGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;

        void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;
    };

    ProxyOwner proxy;
    GoBackN gbn;

    ReliableUdp ( Socket::Owner *owner, unsigned port );
    ReliableUdp ( Socket::Owner *owner, const std::string& address, unsigned port );

public:

    // Listen for connections on the given port
    static std::shared_ptr<Socket> listen ( Socket::Owner *owner, unsigned port );

    // Connect to the given address and port
    static std::shared_ptr<Socket> connect ( Socket::Owner *owner, const std::string& address, unsigned port );

    ~ReliableUdp() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    std::shared_ptr<Socket> accept ( Socket::Owner *owner ) override;

    inline bool isConnected() const override { return false; } // TODO

    void send ( Serializable *message, const IpAddrPort& address = IpAddrPort() ) override;
    void send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;
};

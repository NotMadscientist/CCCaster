
#include <iostream>
#include <string.h>

using namespace std;

#include <netlink/socket.h>
#include <netlink/socket_group.h>

const unsigned SERVER_PORT = 5000;

class OnAccept: public NL::SocketGroupCmd {

	void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

		NL::Socket* newConnection = socket->accept();
		group->add(newConnection);
		cout << "\nConnection " << newConnection->hostTo() << ":" << newConnection->portTo() << " added...";
		cout.flush();
	}
};


class OnRead: public NL::SocketGroupCmd {

	void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

		cout << "\nREAD -- ";
		cout.flush();
		char buffer[256];
		buffer[255] = '\0';
		socket->read(buffer, 255);
		size_t msgLen = strlen(buffer);
		cout << "Message from " << socket->hostTo() << ":" << socket->portTo() << ". Text received: " << buffer;
		cout.flush();
		
		for(unsigned i=1; i < (unsigned) group->size(); ++i)
			if(group->get(i) != socket)
				group->get(i)->send(buffer, msgLen + 1);
	}
};


class OnDisconnect: public NL::SocketGroupCmd {

	void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

		group->remove(socket);
		cout << "\nClient " << socket->hostTo() << " disconnected...";
		cout.flush();
		delete socket;
	}
};

int main() {

	NL::init();

	cout << "\nStarting Server...";
	cout.flush();

	NL::Socket socketServer(SERVER_PORT);

	NL::SocketGroup group;

	OnAccept onAccept;
	OnRead onRead;
	OnDisconnect onDisconnect;

	group.setCmdOnAccept(&onAccept);
	group.setCmdOnRead(&onRead);
	group.setCmdOnDisconnect(&onDisconnect);

	group.add(&socketServer);

	while(true) {

		if(!group.listen(2000))
			cout << "\nNo msg recieved during the last 2 seconds";
	}
	
}

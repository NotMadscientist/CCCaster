
#include <iostream>
#include <string>
#include <string.h>

using namespace std;

#include <netlink/socket.h>
#include <netlink/socket_group.h>

const string HOST = "localhost";
const unsigned SERVER_PORT = 5000;

bool disconnect = false;


class OnRead: public NL::SocketGroupCmd {

	void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

		char buffer[256];
		buffer[255] = '\0';
		socket->read(buffer, 255);
		cout << "\nReceived message: " << buffer;
	}
};

class OnDisconnect: public NL::SocketGroupCmd {

	void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

		disconnect = true;
	}
};


int main() {

	cout << "\nConnecting...";
	cout.flush();

	try {

		NL::init();

		NL::Socket socket(HOST, SERVER_PORT);

		NL::SocketGroup group;
		group.add(&socket);

		OnRead onRead;
		OnDisconnect onDisconnect;

		group.setCmdOnRead(&onRead);
		group.setCmdOnDisconnect(&onDisconnect);

		while(!disconnect) {

			char input[256];
			input[255] = '\0';
			cout << "\n--> ";
			cin.getline(input, 255);

			if(!strcmp(input, "exit"))
				disconnect = true;
			else
				socket.send(input, strlen(input)+1);

			group.listen(500);
		}

	}

	catch(NL::Exception e) {

		cout << "\n***ERROR*** " << e.what();
	}

}

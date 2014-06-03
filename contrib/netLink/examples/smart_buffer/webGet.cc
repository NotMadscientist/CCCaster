#include <netlink/socket.h>
#include <netlink/socket_group.h>
#include <netlink/smart_buffer.h>

#include <string>
#include <iostream>

using std::string;
using std::cout;


class OnRead: public NL::SocketGroupCmd {


	void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

		((NL::SmartBuffer*)reference)->read(socket);
	}

};


class OnDisconnect: public NL::SocketGroupCmd {

	void exec(NL::Socket* socket, NL::SocketGroup* group, void* reference) {

		group->remove(socket);
	}

};




int main(int argc, char *argv[]) {

	if (argc < 2){
		cout << "\n Use:\n\t" << argv[0] << " URL\n";
		return 0; 
	}

	NL::init();

	string url(argv[1]);

	if(!url.substr(0,7).compare("http://"))
		url = url.substr(7);

	size_t slashPosition = url.find('/');
	string host, path;

	if(slashPosition != string::npos) {

		host = url.substr(0, slashPosition);
		path = url.substr(slashPosition);
	}
	else {
		host = url;
		path = "/";
	}

	cout << "\nHost: " << host << "\tPath: " << path;

	try {
	
		NL::Socket socket(host,80);

		string request = "GET " + path + " HTTP/1.0\r\n";
		request += "Host: " + host + "\r\n";
		request += "User Agent: webGet - NetLink Sockets C++ Library Example\r\n\r\n";

		socket.send(request.c_str(), request.length());
		NL::SmartBuffer buffer;

		NL::SocketGroup group;
		group.add(&socket);

		OnRead onRead;
		OnDisconnect onDisconnect;

		group.setCmdOnRead(&onRead);
		group.setCmdOnDisconnect(&onDisconnect);

		while(group.size())
			group.listen(0, &buffer); 

		cout << "\n";
		cout.write((const char*)*buffer, buffer.size());

	}

	catch(NL::Exception e) {

		cout << "\nError: " << e.what();
	}

	return 0;
}

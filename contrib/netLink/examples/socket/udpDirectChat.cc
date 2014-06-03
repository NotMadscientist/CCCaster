#include <iostream>
#include <sstream>
#include <string>
#include <string.h>

using namespace std;

#include <netlink/socket.h>


unsigned getUnsigned(char* input) {

	stringstream ss;
	ss << input;
	unsigned result;
	ss >> result;

	return result;
}


int main(int argc, char *argv[]) {

	if (argc < 4){
		cout << "\n Use:\n\t" << argv[0] << " localPort remoteHost remotePort\n";
		return 0; 
	}

	NL::init();

	unsigned localPort = getUnsigned(argv[1]);
	unsigned remotePort = getUnsigned(argv[3]);	
	string remoteHost(argv[2]);
		
	cout << "\nStarting UDP Connection...";
	cout.flush();
	try {
		NL::Socket socket(remoteHost, remotePort, localPort);
		socket.blocking(false);

	
		char buffer[256];
		buffer[255] = '\0';

		while(true) {

			cout << "\n--> ";
			cin.getline(buffer, 255);
			socket.sendTo(buffer, strlen(buffer)+1, remoteHost, remotePort);
			if(!strcmp(buffer, "exit"))
				break;

			string remoteHost;
			unsigned remotePort;		

			if(socket.readFrom(buffer, 255, &remoteHost, &remotePort) != -1)
				cout << "\nRcv From " << remoteHost << ":" << remotePort << ". Text: " << buffer;
		}

		cout << "\nEnd";
	}

	catch(NL::Exception e) {

		cout << "***ERROR*** " << e.what();
		return 0;
	}


}

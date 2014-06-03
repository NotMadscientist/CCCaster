
#include <iostream>
#include <string.h>
using namespace std;

#include <netlink/socket.h>


int main() {

	NL::init();

	cout << "\nEcho Client...";
	cout.flush();

	try {

		NL::Socket socket("localhost", 5000);

		char input[256];
		input[255] = '\0';

		while(strcmp(input, "exit")) {

			cout << "\n--> ";
			cin.getline(input, 255);
			socket.send(input, strlen(input) + 1);
		}
	}

	catch(NL::Exception e) {

		cout << "\n***Error*** " << e.what();
	}

	return 0;
}

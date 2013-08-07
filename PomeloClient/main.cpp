#include <memory>
#include "PomeloCpp/Client.h"

const char *ip = "127.0.0.1";
int port = 3010;

int main() {
	PomeloCpp::Client client(ip, port);
	client.on([] (PomeloCpp::ConnectReq &req, int status) { cout << "Connected." << endl; } );

	client.connect(true, true);

	while(!client.isConnected())
		Sleep(1);

	client.notify("connector.entryHandler.entry",
		Json::Object("msg", "hehe"),  
		[] (PomeloCpp::NotifyReq &req, int status) { cout << "Hello world" << endl; } 
	);
	client.request("connector.entryHandler.entry",
		Json::Object("msg", "gege"),
		[] (PomeloCpp::RequestReq &req, int status, Json::Value resp) 
		{
			if(status == -1) {
				printf("Fail to send request to server.\n");
			} else if(status == 0) {
				std::shared_ptr<Json::Writer> writer(new Json::FastWriter());
				cout << writer->write(resp) << endl;
			}
		}
	);

	getchar();

	client.disConnect();

	return 0;
}

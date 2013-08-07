#include <memory>
#include "PomeloCpp/Client.h"

const char *ip = "127.0.0.1";
int port = 3010;

int main() {
	PomeloCpp::Client client(ip, port);
	client.connect();

	while(!client.isConnected()) {
		client.logic();
		Sleep(1);
	}

	Json::Value msg;
	msg["msg"] = "hehe";

	client.notify("connector.entryHandler.entry",
		msg,  
		[] (PomeloCpp::NotifyReq &req, int status) { cout << "Hello world" << endl; } );
	client.request("connector.entryHandler.entry",
		msg,
		[] (PomeloCpp::RequestReq &req, int status, Json::Value resp) {
			if(status == -1) {
				printf("Fail to send request to server.\n");
			} else if(status == 0) {
				std::shared_ptr<Json::Writer> writer(new Json::FastWriter());
				cout << writer->write(resp) << endl;
			}
	});
	client.on([] (PomeloCpp::ConnectReq &req, int status) { cout << "Connected." << endl; } );

	while(client.isConnected())
	{
		client.logic();
		Sleep(1);
	}

	client.disConnect();

	return 0;
}

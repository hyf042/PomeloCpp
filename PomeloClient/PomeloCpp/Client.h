#ifndef _POMELOCPP_CLIENT_H_
#define _POMELOCPP_CLIENT_H_

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <memory>
#include "PreDeclare.h"
#include "Event.h"
#include "Json.h"
#include "ReqWrapper.h"

using namespace std;

namespace PomeloCpp
{
	class Address
	{
	public:
		string ip;
		int port;
	};

	class Client
	{
		typedef Base::Function<void(NotifyReq&, int)> notify_cb;
		typedef Base::Function<void(RequestReq&, int, Json::Value)> request_cb;
		typedef Base::Function<void(MessageReq&)> message_cb;
		typedef Base::Function<void(ConnectReq&, int)> connected_cb;
		typedef Base::Publisher<void(MessageReq&)> message_pub;
		typedef Base::Publisher<void(ConnectReq&, int)> connected_pub;
		
		typedef unsigned long LONG;

		pc_client_t* client;

		Event lastEvent;
		bool connected;
		Address address;
		Base::ThreadSafe::Queue<Event> events;
		map<LONG, request_cb> requestCbs;
		map<LONG, notify_cb> notifyCbs;
		map<string, message_pub> messagePubs;
		connected_pub _connected_pub;

		pc_msg_parse_done_cb default_parse_done;

	public:

		Client(string ip = "127.0.0.1", int port = 3010)
		{
			this->address.ip = ip;
			this->address.port = port;
			connected = false;
		}
		~Client() {disConnect();}

		/*
		 * interface
		 */
		bool connect(bool async = true)
		{
			if (connected)
				disConnect();

			client = pc_client_new();
			client->data = (void*)this;

			struct sockaddr_in address;

			memset(&address, 0, sizeof(struct sockaddr_in));
			address.sin_family = AF_INET;
			address.sin_port = htons(this->address.port);
			address.sin_addr.s_addr = inet_addr(this->address.ip.c_str());

			default_parse_done = client->parse_msg_done;
			client->parse_msg_done = pc_msg_parse_done_cb;
			pc_add_listener(client, PC_EVENT_DISCONNECT, on_close_cb);

			if (async) {
				pc_connect_t *conn_req = pc_connect_req_new(&address);
				//~ bring some private date on
				conn_req->data = (void*)this;

				if(pc_client_connect2(client, conn_req, on_connected_cb)) {
					BASE_LOGGER.log_error("fail to connect server.\n");
					pc_connect_req_destroy(conn_req);
					pc_client_destroy(client);
					return false;
				}
			}
			else {
				if(pc_client_connect(client, &address)) {
					BASE_LOGGER.log_error("fail to connect server.\n");
					pc_client_destroy(client);
					return false;
				}
				connected = true;
			}

			return true;
		}
		void disConnect()
		{
			if (!connected)
				return;

			pc_client_destroy(client), client = NULL;
			connected = false;
		}
		void join() {
			if (!isConnected())
				return;

			pc_client_join(client);
		}
		void logic()
		{
			Event ev;
			while (events.take(ev))
			{
				lastEvent = ev;
				switch (ev.type)
				{
				case Event::Type::OnClose:
					on_close(ev.event);
					break;
				case Event::Type::OnConnect:
					on_connected(ev.req.conn_req, ev.status);
					break;
				case Event::Type::OnRequest:
					on_request(ev.req.request_req, ev.status, ev.resp);
					break;
				case Event::Type::OnNotified:
					on_notified(ev.req.notify_req, ev.status);
					break;
				case Event::Type::OnClear:
					on_clear(ev.req.msg);
					break;
				default:
					BASE_LOGGER.log_warning("[Libpomelo.Client] Unknown Event.");
					break;
				}
			}
		}

		void request(std::string route, Json::Value msg, request_cb cb) {
			pc_request_t *request = pc_request_new();
			request->data = (void*)this;
			if (!cb.empty())
				requestCbs[(LONG)request] =  cb;

			pc_request(client, request, route.c_str(), JsonHelper::translate(msg), on_request_cb);
		}
		void notify(std::string route, Json::Value msg, notify_cb cb) {
			pc_notify_t *notify = pc_notify_new();
			notify->data = (void*)this;
			if (!cb.empty())
				notifyCbs[(LONG)notify] =  cb;

			pc_notify(client, notify, route.c_str(), JsonHelper::translate(msg), on_notified_cb);
		}

		/*
		 * event register/unregister
		 */
		void on(std::string route, message_cb cb) {
			if (!cb.empty()) {
				messagePubs[route] += cb;
			}
		}
		void on(connected_cb cb) {
			_connected_pub += cb;
		}
		void once(std::string route, message_cb cb) {
			if (!cb.empty()) {
				messagePubs[route].once(cb);
			}
		}
		void once(connected_cb cb) {
			_connected_pub.once(cb);
		}
		void remove(std::string route, message_cb cb) {
			messagePubs[route] -= cb;
		}
		void remove(connected_cb cb) {
			_connected_pub -= cb;
		}
		
		/*
		 * Get&Set Functions
		 */
		bool isConnected() const  {
			return connected;
		}
		Event getLastEvent() const {return lastEvent;}

	private:
		/*
		 * Callback Functions
		 */
		void on_close(std::string event)
		{
			BASE_LOGGER.log("Client closed.");

			_callMessageCb("disconnect", event);
		}
		void on_connected(pc_connect_t *conn_req, int status)
		{
			BASE_LOGGER.log(Base::String::format("[iPomelo]Client Connected: data = %p", conn_req->data));
			if (status == 0)
				connected = true;

			_connected_pub(ConnectReq(conn_req, this), status);

			pc_connect_req_destroy(conn_req);
		}
		void on_request(pc_request_t *req, int status, json_t *resp) {
			BASE_LOGGER.log(Base::String::format("[iPomelo]Client Request Response: %s", req->route));

			Json::Value json = JsonHelper::translate(resp);
			if(requestCbs.find((LONG)req) != requestCbs.end()) {
				requestCbs[(LONG)req](RequestReq(req, this), status, json);
			}

			// release relative resource with pc_request_t
			json_t *msg = req->msg;
			json_decref(msg);
			pc_request_destroy(req);
		}
		void on_notified(pc_notify_t *req, int status) {
			BASE_LOGGER.log(Base::String::format("[iPomelo]Client Notify Response: %s", req->route));

			if(notifyCbs.find((LONG)req) != notifyCbs.end()) {
				notifyCbs[(LONG)req](NotifyReq(req, this), status);
				notifyCbs.erase((LONG)req);
			}

			// release resources
			json_t *msg = req->msg;
			json_decref(msg);
			pc_notify_destroy(req);
		}
		void on_clear(pc_msg_t *msg) {
			default_parse_done(client, msg);
		}

		/*
		 * Callback Static Functions
		 */
		static void on_close_cb(pc_client_t *client, const char *event, void *data)
		{
			static_cast<Client*>(client->data)->events.push(Event::CreateOnCloseEvent(event));
		}
		static void on_connected_cb(pc_connect_t *conn_req, int status){
			static_cast<Client*>(conn_req->data)->events.push(Event::CreateOnConnectEvent(conn_req, status));
		}
		static void on_request_cb(pc_request_t *req, int status, json_t *resp){
			static_cast<Client*>(req->data)->events.push(Event::CreateOnRequestEvent(req, status, resp));
		}
		static void on_notified_cb(pc_notify_t *req, int status)
		{
			static_cast<Client*>(req->data)->events.push(Event::CreateOnNotifiedEvent(req, status));
		}

		static void pc_msg_parse_done_cb(pc_client_t *client, pc_msg_t *msg) {
			static_cast<Client*>(client->data)->events.push(Event::CreateOnClearEvent(msg));
		}
	
	private:
		bool _callMessageCb(std::string route, std::string event = "", void *data = 0) {
			MessageReq req(route, event, data, this); 
			if (messagePubs.find(route) == messagePubs.end())
				return false;
			messagePubs[route](req);
			return true;
		}
	};
}

#endif
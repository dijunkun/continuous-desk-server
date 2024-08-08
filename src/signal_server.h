#ifndef _SIGNAL_SERVER_H_
#define _SIGNAL_SERVER_H_

#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "client_id_generator.h"
#include "transmission_manager.h"

using nlohmann::json;

typedef websocketpp::server<websocketpp::config::asio> server;
typedef unsigned int connection_id;
typedef std::string room_id;

class SignalServer {
 public:
  SignalServer();
  ~SignalServer();

  bool on_open(websocketpp::connection_hdl hdl);

  bool on_close(websocketpp::connection_hdl hdl);

  bool on_ping(websocketpp::connection_hdl hdl, std::string s);

  bool on_pong(websocketpp::connection_hdl hdl, std::string s);

  void run(uint16_t port);

  void on_message(websocketpp::connection_hdl hdl, server::message_ptr msg);

  void send_msg(websocketpp::connection_hdl hdl, json message);

 private:
  server server_;
  std::map<websocketpp::connection_hdl, connection_id,
           std::owner_less<websocketpp::connection_hdl>>
      ws_connections_;
  unsigned int ws_connection_id_ = 0;

 private:
  TransmissionManager transmission_manager_;
  ClientIdGenerator client_id_generator_;
};

#endif
#include "signal_server.h"

#include "common.h"
#include "log.h"

const std::string GenerateTransmissionId() {
  static const char alphanum[] = "0123456789";
  std::string random_id;
  random_id.reserve(6);

  for (int i = 0; i < 6; ++i) {
    random_id += alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  return "000000";
}

SignalServer::SignalServer() {
  // Set logging settings
  server_.set_error_channels(websocketpp::log::elevel::all);
  server_.set_access_channels(websocketpp::log::alevel::none);

  // Initialize Asio
  server_.init_asio();

  server_.set_open_handler(
      std::bind(&SignalServer::on_open, this, std::placeholders::_1));

  server_.set_close_handler(
      std::bind(&SignalServer::on_close, this, std::placeholders::_1));

  server_.set_message_handler(std::bind(&SignalServer::on_message, this,
                                        std::placeholders::_1,
                                        std::placeholders::_2));

  server_.set_ping_handler(bind(&SignalServer::on_ping, this,
                                std::placeholders::_1, std::placeholders::_2));

  server_.set_pong_handler(bind(&SignalServer::on_pong, this,
                                std::placeholders::_1, std::placeholders::_2));
}

SignalServer::~SignalServer() {}

bool SignalServer::on_open(websocketpp::connection_hdl hdl) {
  ws_connections_[hdl] = ws_connection_id_++;

  LOG_INFO("New websocket connection [{}] established", ws_connection_id_);

  // json message = {{"type", "ws_connection_id"},
  //                 {"ws_connection_id", ws_connection_id_}};
  // server_.send(hdl, message.dump(), websocketpp::frame::opcode::text);

  return true;
}

bool SignalServer::on_close(websocketpp::connection_hdl hdl) {
  std::string user_id = transmission_manager_.ReleaseUserFromeWsHandle(hdl);
  if (!user_id.empty()) {
    LOG_INFO("Websocket onnection [{}|{}] closed", ws_connections_[hdl],
             user_id);

    std::string transmission_id = transmission_manager_.IsHost(user_id);

    if (!transmission_id.empty()) {
      transmission_manager_.ReleaseTransmission(transmission_id);
      LOG_INFO("Release transmission [{}] due to host leaves", transmission_id);
    } else {
      transmission_id = transmission_manager_.IsGuest(user_id);
    }

    if (!transmission_id.empty()) {
      json message = {{"type", "user_leave_transmission"},
                      {"transmission_id", transmission_id},
                      {"user_id", user_id}};

      std::vector<std::string> user_id_list =
          transmission_manager_.GetAllUserIdOfTransmission(transmission_id);

      for (const auto& user_id : user_id_list) {
        send_msg(transmission_manager_.GetWsHandle(user_id), message);
      }
    }
  }

  ws_connections_.erase(hdl);

  return true;
}

bool SignalServer::on_ping(websocketpp::connection_hdl hdl, std::string s) {
  transmission_manager_.UpdateWsHandleLastActiveTime(hdl);
  return true;
}

bool SignalServer::on_pong(websocketpp::connection_hdl hdl, std::string s) {
  return true;
}

void SignalServer::run(uint16_t port) {
  LOG_INFO("Signal server runs on port [{}]", port);

  server_.set_reuse_addr(true);
  server_.listen(port);

  // Queues a connection accept operation
  server_.start_accept();

  // Start the Asio io_service run loop
  server_.run();
}

void SignalServer::send_msg(websocketpp::connection_hdl hdl, json message) {
  if (!hdl.expired()) {
    server_.send(hdl, message.dump(), websocketpp::frame::opcode::text);
  } else {
    LOG_ERROR("Destination hdl invalid");
  }
}

void SignalServer::on_message(websocketpp::connection_hdl hdl,
                              server::message_ptr msg) {
  transmission_manager_.UpdateWsHandleLastActiveTime(hdl);
  std::string payload = msg->get_payload();

  auto j = json::parse(payload);
  std::string type = j["type"].get<std::string>();

  switch (HASH_STRING_PIECE(type.c_str())) {
    case "login"_H: {
      std::string host_id = j["user_id"].get<std::string>();
      if (host_id.empty()) {
        host_id = client_id_generator_.GeneratorNewId();
        LOG_INFO("New client, assign id [{}] to it", host_id);
      }

      LOG_INFO("Receive host id [{}] login request with id [{}]", host_id);
      bool success = transmission_manager_.BindUserToWsHandle(host_id, hdl);
      if (success) {
        json message = {
            {"type", "login"}, {"user_id", host_id}, {"status", "success"}};
        send_msg(hdl, message);
      } else {
        json message = {
            {"type", "login"}, {"user_id", host_id}, {"status", "fail"}};
        send_msg(hdl, message);
      }

      break;
    }
    case "create_transmission"_H: {
      std::string transmission_id = j["transmission_id"].get<std::string>();
      std::string password = j["password"].get<std::string>();
      std::string host_id = j["user_id"].get<std::string>();

      LOG_INFO("Receive host id [{}] create transmission request with id [{}]",
               host_id, transmission_id);
      if (!transmission_manager_.IsTransmissionExist(transmission_id)) {
        if (transmission_id.empty()) {
          transmission_id = GenerateTransmissionId();
          while (transmission_manager_.IsTransmissionExist(transmission_id)) {
            transmission_id = GenerateTransmissionId();
          }
          LOG_INFO(
              "Transmission id is empty, generate a new one for this request "
              "[{}]",
              transmission_id);
        }

        transmission_manager_.BindHostToTransmission(host_id, transmission_id);
        transmission_manager_.BindPasswordToTransmission(password,
                                                         transmission_id);

        LOG_INFO("Create transmission id [{}]", transmission_id);
        json message = {{"type", "transmission_id"},
                        {"transmission_id", transmission_id},
                        {"status", "success"}};
        send_msg(hdl, message);
      } else {
        LOG_INFO("Transmission id [{}] already exist", transmission_id);
        json message = {{"type", "transmission_id"},
                        {"transmission_id", transmission_id},
                        {"status", "fail"},
                        {"reason", "Transmission id exist"}};
        send_msg(hdl, message);
      }

      break;
    }
    case "leave_transmission"_H: {
      std::string transmission_id = j["transmission_id"].get<std::string>();
      std::string user_id = j["user_id"].get<std::string>();
      LOG_INFO("[{}] leaves transmission [{}]", user_id.c_str(),
               transmission_id.c_str());

      json message = {{"type", "user_leave_transmission"},
                      {"transmission_id", transmission_id},
                      {"user_id", user_id}};

      std::vector<std::string> user_id_list =
          transmission_manager_.GetAllUserIdOfTransmission(transmission_id);

      for (const auto& user_id : user_id_list) {
        send_msg(transmission_manager_.GetWsHandle(user_id), message);
      }

      bool is_host =
          transmission_manager_.IsHostOfTransmission(user_id, transmission_id);

      if (is_host) {
        transmission_manager_.ReleaseTransmission(transmission_id);
        LOG_INFO("Release transmission [{}] due to host leaves",
                 transmission_id);
      } else {
        transmission_manager_.ReleaseGuestFromTransmission(user_id);
      }

      break;
    }
    case "query_user_id_list"_H: {
      std::string transmission_id = j["transmission_id"].get<std::string>();
      std::string password = j["password"].get<std::string>();

      int ret = transmission_manager_.CheckPassword(password, transmission_id);

      if (0 == ret) {
        std::vector<std::string> user_id_list =
            transmission_manager_.GetAllUserIdOfTransmission(transmission_id);

        json message = {{"type", "user_id_list"},
                        {"transmission_id", transmission_id},
                        {"user_id_list", user_id_list},
                        {"status", "success"}};

        send_msg(hdl, message);
      } else if (-1 == ret) {
        std::vector<std::string> user_id_list;
        json message = {{"type", "user_id_list"},
                        {"transmission_id", transmission_id},
                        {"user_id_list", user_id_list},
                        {"status", "failed"},
                        {"reason", "Incorrect password"}};
        // LOG_INFO(
        //     "Incorrect password [{}] for transmission [{}] with password is "
        //     "[{}]",
        //     password, transmission_id,
        //     transmission_manager_.GetPassword(transmission_id));

        send_msg(hdl, message);
      } else if (-2 == ret) {
        std::vector<std::string> user_id_list;
        json message = {{"type", "user_id_list"},
                        {"transmission_id", transmission_id},
                        {"user_id_list", user_id_list},
                        {"status", "failed"},
                        {"reason", "No such transmission id"}};
        // LOG_INFO(
        //     "Incorrect password [{}] for transmission [{}] with password is "
        //     "[{}]",
        //     password, transmission_id,
        //     transmission_manager_.GetPassword(transmission_id));

        send_msg(hdl, message);
      }

      // LOG_INFO("Send member_list: [{}]", message.dump());

      break;
    }
    case "offer"_H: {
      std::string transmission_id = j["transmission_id"].get<std::string>();
      std::string remote_user_id = j["remote_user_id"].get<std::string>();
      std::string user_id = j["user_id"].get<std::string>();

      transmission_manager_.BindGuestToTransmission(user_id, transmission_id);

      websocketpp::connection_hdl destination_hdl =
          transmission_manager_.GetWsHandle(remote_user_id);

      if (j.contains("sdp")) {
        std::string sdp = j["sdp"].get<std::string>();
        json message = {
            {"type", "offer"},
            {"transmission_id", transmission_id},
            {"remote_user_id", user_id},
            {"sdp", sdp},
        };
        LOG_INFO("[{}] send offer to [{}]", user_id, remote_user_id);
        send_msg(destination_hdl, message);

      } else {
        LOG_ERROR("Invalid offer msg");
      }

      break;
    }
    case "answer"_H: {
      std::string transmission_id = j["transmission_id"].get<std::string>();
      std::string remote_user_id = j["remote_user_id"].get<std::string>();
      std::string user_id = j["user_id"].get<std::string>();

      websocketpp::connection_hdl destination_hdl =
          transmission_manager_.GetWsHandle(remote_user_id);

      if (j.contains("sdp")) {
        std::string sdp = j["sdp"].get<std::string>();
        json message = {{"type", "answer"},
                        {"sdp", sdp},
                        {"remote_user_id", user_id},
                        {"transmission_id", transmission_id}};
        LOG_INFO("[{}] send answer to [{}]", user_id, remote_user_id);
        send_msg(destination_hdl, message);
      } else {
        LOG_ERROR("Invalid answer msg");
      }

      break;
    }
    case "new_candidate"_H: {
      std::string transmission_id = j["transmission_id"].get<std::string>();
      std::string candidate = j["sdp"].get<std::string>();
      std::string user_id = j["user_id"].get<std::string>();
      std::string remote_user_id = j["remote_user_id"].get<std::string>();

      websocketpp::connection_hdl destination_hdl =
          transmission_manager_.GetWsHandle(remote_user_id);

      // LOG_INFO("send candidate [{}]", candidate.c_str());
      json message = {{"type", "new_candidate"},
                      {"sdp", candidate},
                      {"remote_user_id", user_id},
                      {"transmission_id", transmission_id}};
      send_msg(destination_hdl, message);
      break;
    }
    default:
      break;
  }

  // std::string sdp = j["sdp"];

  // LOG_INFO("Message type: {}", type);
  // LOG_INFO("Message body: {}", sdp);

  // server_.send(hdl, msg->get_payload(), msg->get_opcode());
}
#include "transmission_manager.h"

#include "log.h"

TransmissionManager::TransmissionManager() {}

TransmissionManager::~TransmissionManager() {}

std::vector<std::string> TransmissionManager::GetAllUserIdOfTransmission(
    const std::string& transmission_id) {
  std::vector<std::string> user_id_list;
  if (transmission_host_id_list_.find(transmission_id) !=
      transmission_host_id_list_.end()) {
    auto host_id = transmission_host_id_list_[transmission_id];
    user_id_list.push_back(host_id);
  }

  if (transmission_guest_id_list_.find(transmission_id) !=
      transmission_guest_id_list_.end()) {
    auto guest_id_list = transmission_guest_id_list_[transmission_id];
    user_id_list.insert(user_id_list.end(), guest_id_list.begin(),
                        guest_id_list.end());
  }

  return user_id_list;
}

bool TransmissionManager::BindHostIdToTransmission(
    const std::string& host_id, const std::string& transmission_id) {
  if (transmission_host_id_list_.find(transmission_id) ==
      transmission_host_id_list_.end()) {
    transmission_host_id_list_[transmission_id] = host_id;
    LOG_INFO("Bind host id [{}]  to transmission [{}]", host_id,
             transmission_id);
    return true;
  } else {
    LOG_WARN("Host id [{}] already bind to transmission [{}]", host_id,
             transmission_id);
    return false;
  }
  return true;
}

bool TransmissionManager::BindGuestIdToTransmission(
    const std::string& guest_id, const std::string& transmission_id) {
  if (transmission_guest_id_list_.find(transmission_id) ==
      transmission_guest_id_list_.end()) {
    transmission_guest_id_list_[transmission_id].push_back(guest_id);
    LOG_INFO("Bind guest id [{}]  to transmission [{}]", guest_id,
             transmission_id);
    return true;
  } else {
    auto guest_id_list = transmission_guest_id_list_[transmission_id];
    for (auto id : guest_id_list) {
      if (id == guest_id) {
        LOG_WARN("Guest id [{}] already bind to transmission [{}]", guest_id,
                 transmission_id);
        return false;
      }
    }
    transmission_guest_id_list_[transmission_id].push_back(guest_id);
    LOG_INFO("Bind guest id [{}]  to transmission [{}]", guest_id,
             transmission_id);
  }
  return true;
}

bool TransmissionManager::BindPasswordToTransmission(
    const std::string& password, const std::string& transmission_id) {
  if (transmission_password_list_.find(transmission_id) ==
      transmission_password_list_.end()) {
    transmission_password_list_[transmission_id] = password;
    // LOG_INFO("Bind password [{}]  to transmission [{}]", password,
    //          transmission_id);
    return true;
  } else {
    auto old_password = transmission_password_list_[transmission_id];
    transmission_password_list_[transmission_id] = password;
    // LOG_WARN("Update password [{}]  to [{}] for transmission [{}]",
    //          old_password, password, transmission_id);
    return true;
  }

  return false;
}

bool TransmissionManager::BindUserIdToWsHandle(
    const std::string& user_id, websocketpp::connection_hdl hdl) {
  if (user_id_ws_hdl_list_.find(user_id) != user_id_ws_hdl_list_.end()) {
    LOG_WARN("User id [{}] already bind to websocket handle [{} | now {}]",
             user_id, user_id_ws_hdl_list_[user_id].lock().get(),
             hdl.lock().get());
    return false;
  } else {
    user_id_ws_hdl_list_[user_id] = hdl;
  }
  return true;
}

bool TransmissionManager::IsHostOfTransmission(
    const std::string& user_id, const std::string& transmission_id) {
  if (transmission_host_id_list_.find(transmission_id) ==
      transmission_host_id_list_.end()) {
    return false;
  }
  return transmission_host_id_list_[transmission_id] == user_id;
}

std::string TransmissionManager::ReleaseGuestIdFromTransmission(
    websocketpp::connection_hdl hdl) {
  for (auto it = user_id_ws_hdl_list_.begin(); it != user_id_ws_hdl_list_.end();
       ++it) {
    if (it->second.lock().get() == hdl.lock().get()) {
      for (auto trans_it = transmission_guest_id_list_.begin();
           trans_it != transmission_guest_id_list_.end(); ++trans_it) {
        auto& guest_id_list = trans_it->second;
        auto guest_id_it =
            std::find(guest_id_list.begin(), guest_id_list.end(), it->first);
        if (guest_id_it != guest_id_list.end()) {
          guest_id_list.erase(guest_id_it);
          LOG_INFO("Remove guest id [{}] from transmission [{}]", it->first,
                   trans_it->first);
          user_id_ws_hdl_list_.erase(it);
          return trans_it->first;
        }
      }
    }
  }
  return "";
}

bool TransmissionManager::ReleaseAllUserIdFromTransmission(
    const std::string& transmission_id) {
  if (transmission_guest_id_list_.end() !=
      transmission_guest_id_list_.find(transmission_id)) {
    auto guest_id_list = transmission_guest_id_list_[transmission_id];
    for (auto& guest_id : guest_id_list) {
      if (user_id_ws_hdl_list_.find(guest_id) != user_id_ws_hdl_list_.end()) {
        LOG_INFO("Remove user id [{}] from transmission [{}]", guest_id,
                 transmission_id);
        user_id_ws_hdl_list_.erase(guest_id);
      }
    }

    transmission_guest_id_list_.erase(transmission_id);
  }

  if (transmission_host_id_list_.end() !=
      transmission_host_id_list_.find(transmission_id)) {
    auto host_id = transmission_host_id_list_[transmission_id];
    if (user_id_ws_hdl_list_.find(host_id) != user_id_ws_hdl_list_.end()) {
      LOG_INFO("Remove user id [{}] from transmission [{}]", host_id,
               transmission_id);
      user_id_ws_hdl_list_.erase(host_id);
    }

    transmission_host_id_list_.erase(transmission_id);
  }
  return true;
}

bool TransmissionManager::ReleasePasswordFromTransmission(
    const std::string& transmission_id) {
  if (transmission_password_list_.end() ==
      transmission_password_list_.find(transmission_id)) {
    LOG_ERROR("No transmission with id [{}]", transmission_id);
    return false;
  }

  transmission_password_list_.erase(transmission_id);

  return true;
}

websocketpp::connection_hdl TransmissionManager::GetWsHandle(
    const std::string& user_id) {
  if (user_id_ws_hdl_list_.find(user_id) != user_id_ws_hdl_list_.end()) {
    return user_id_ws_hdl_list_[user_id];
  } else {
    websocketpp::connection_hdl hdl;
    return hdl;
  }
}

std::string TransmissionManager::GetUserId(websocketpp::connection_hdl hdl) {
  for (auto it = user_id_ws_hdl_list_.begin(); it != user_id_ws_hdl_list_.end();
       ++it) {
    // LOG_INFO("[{}]", it->first);
    if (it->second.lock().get() == hdl.lock().get()) return it->first;
  }
  return "";
}

int TransmissionManager::CheckPassword(const std::string& password,
                                       const std::string& transmission_id) {
  if (transmission_password_list_.find(transmission_id) ==
      transmission_password_list_.end()) {
    LOG_ERROR("No transmission with id [{}]", transmission_id);
    return -2;
  }

  return transmission_password_list_[transmission_id] == password ? 0 : -1;
}

std::string TransmissionManager::GetPassword(
    const std::string& transmission_id) {
  if (transmission_password_list_.find(transmission_id) ==
      transmission_password_list_.end()) {
    LOG_ERROR("No transmission with id [{}]", transmission_id);
    return "";
  }

  return transmission_password_list_[transmission_id];
}
#include "transmission_manager.h"

#include "log.h"

TransmissionManager::TransmissionManager() {
  std::thread t(&TransmissionManager::AliveChecker, this);
  ws_hdl_alive_checker_ = std::move(t);
}

TransmissionManager::~TransmissionManager() {
  if (ws_hdl_alive_checker_.joinable()) {
    ws_hdl_alive_checker_.join();
  }
}

bool TransmissionManager::IsTransmissionExist(
    const std::string& transmission_id) {
  if (transmission_host_id_list_.find(transmission_id) !=
      transmission_host_id_list_.end()) {
    return true;
  } else {
    return false;
  }
}

bool TransmissionManager::ReleaseTransmission(
    const std::string& transmission_id) {
  std::lock_guard<std::recursive_mutex> lock(ws_hdl_alive_checker_mutex_);
  if (transmission_guest_id_list_.end() !=
      transmission_guest_id_list_.find(transmission_id)) {
    auto guest_id_list = transmission_guest_id_list_[transmission_id];
    for (auto& guest_id : guest_id_list) {
      auto hdl = GetWsHandle(guest_id);
      if (ws_hdl_iter_list_.find(hdl) != ws_hdl_iter_list_.end()) {
        auto iter = ws_hdl_iter_list_[hdl];
        ws_hdl_last_active_time_list_.erase(iter);
        ws_hdl_iter_list_.erase(hdl);
      }
    }

    transmission_guest_id_list_.erase(transmission_id);
  }

  if (transmission_host_id_list_.end() !=
      transmission_host_id_list_.find(transmission_id)) {
    auto host_id = transmission_host_id_list_[transmission_id];
    auto hdl = GetWsHandle(host_id);
    if (ws_hdl_iter_list_.find(hdl) != ws_hdl_iter_list_.end()) {
      auto iter = ws_hdl_iter_list_[hdl];
      ws_hdl_last_active_time_list_.erase(iter);
      ws_hdl_iter_list_.erase(hdl);
    }
    transmission_host_id_list_.erase(transmission_id);
  }

  if (transmission_password_list_.end() !=
      transmission_password_list_.find(transmission_id)) {
    transmission_password_list_.erase(transmission_id);
  }

  return true;
}

std::string TransmissionManager::IsHost(const std::string& user_id) {
  if (transmission_host_id_list_.find(user_id) !=
      transmission_host_id_list_.end()) {
    return transmission_host_id_list_[user_id];
  } else {
    return "";
  }
}

std::string TransmissionManager::IsGuest(const std::string& user_id) {
  for (auto& transmission_id : transmission_guest_id_list_) {
    auto guest_id_list = transmission_id.second;
    for (auto& guest_id : guest_id_list) {
      if (guest_id == user_id) {
        return transmission_id.first;
      }
    }
  }

  return "";
}

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

bool TransmissionManager::BindHostToTransmission(
    const std::string& host_id, const std::string& transmission_id) {
  if (transmission_host_id_list_.find(transmission_id) ==
      transmission_host_id_list_.end()) {
    transmission_host_id_list_[transmission_id] = host_id;
    LOG_INFO("Bind host id [{}] to transmission [{}]", host_id,
             transmission_id);
    return true;
  } else {
    LOG_WARN("Host id [{}] already bind to transmission [{}]", host_id,
             transmission_id);
    return false;
  }
  return true;
}

bool TransmissionManager::BindGuestToTransmission(
    const std::string& guest_id, const std::string& transmission_id) {
  if (transmission_guest_id_list_.find(transmission_id) ==
      transmission_guest_id_list_.end()) {
    transmission_guest_id_list_[transmission_id].push_back(guest_id);
    LOG_INFO("Bind guest id [{}] to transmission [{}]", guest_id,
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

bool TransmissionManager::BindUserToWsHandle(const std::string& user_id,
                                             websocketpp::connection_hdl hdl) {
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

std::string TransmissionManager::ReleaseUserFromeWsHandle(
    websocketpp::connection_hdl hdl) {
  std::string user_id;
  for (auto it = user_id_ws_hdl_list_.begin(); it != user_id_ws_hdl_list_.end();
       ++it) {
    if (it->second.lock().get() == hdl.lock().get()) {
      user_id = it->first;
      user_id_ws_hdl_list_.erase(it);
      break;
    }
  }

  return user_id;
}

bool TransmissionManager::IsHostOfTransmission(
    const std::string& user_id, const std::string& transmission_id) {
  if (transmission_host_id_list_.find(transmission_id) ==
      transmission_host_id_list_.end()) {
    return false;
  }
  return transmission_host_id_list_[transmission_id] == user_id;
}

bool TransmissionManager::ReleaseGuestFromTransmission(
    const std::string& guest_id) {
  for (auto trans_it = transmission_guest_id_list_.begin();
       trans_it != transmission_guest_id_list_.end(); ++trans_it) {
    auto& guest_id_list = trans_it->second;
    auto guest_id_it =
        std::find(guest_id_list.begin(), guest_id_list.end(), guest_id);
    if (guest_id_it != guest_id_list.end()) {
      // LOG_INFO("Remove guest id [{}] from transmission [{}]", guest_id,
      //          trans_it->first);
      guest_id_list.erase(guest_id_it);
      return true;
    }
  }

  LOG_ERROR("Guest id [{}] not found in transmission list", guest_id);

  return false;
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

/*Lifetime*/
int TransmissionManager::UpdateWsHandleLastActiveTime(
    websocketpp::connection_hdl hdl) {
  // if already record last active time
  std::lock_guard<std::recursive_mutex> lock(ws_hdl_alive_checker_mutex_);

  if (ws_hdl_iter_list_.find(hdl) != ws_hdl_iter_list_.end()) {
    auto it = ws_hdl_iter_list_[hdl];
    if (it != ws_hdl_last_active_time_list_.end()) {
      ws_hdl_last_active_time_list_.erase(it);
    }
  }

  uint32_t now_time = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
  ws_hdl_last_active_time_list_.push_front(std::make_pair(hdl, now_time));
  ws_hdl_iter_list_[hdl] = ws_hdl_last_active_time_list_.begin();

  // LOG_INFO("Update [{}] with time [{}]", hdl.lock().get(), now_time);

  return 0;
}

void TransmissionManager::AliveChecker() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::lock_guard<std::recursive_mutex> lock(ws_hdl_alive_checker_mutex_);
    while (!ws_hdl_last_active_time_list_.empty()) {
      auto hdl = ws_hdl_last_active_time_list_.back().first;
      if (hdl.expired()) {
        break;
      }

      uint32_t now_time =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();

      uint32_t duration =
          now_time - ws_hdl_last_active_time_list_.back().second;

      bool is_dead = duration > 100000000 ? true : false;

      if (is_dead) {
        LOG_INFO(
            "Websocket handle [{}] is dead, now time [{}], last active time "
            "[{}], duration [{}]",
            hdl.lock().get(), now_time,
            ws_hdl_last_active_time_list_.back().second, duration);
        if (ws_hdl_iter_list_.find(hdl) != ws_hdl_iter_list_.end()) {
          auto it = ws_hdl_iter_list_[hdl];

          auto user_id = GetUserId(hdl);
          auto transmission_id = IsHost(user_id);
          if (transmission_id.empty()) {
            LOG_INFO("Host [{}|{}] is dead, release transmission [{}]", user_id,
                     hdl.lock().get(), transmission_id);
            ReleaseTransmission(transmission_id);
          }
          ws_hdl_last_active_time_list_.pop_back();
          ws_hdl_iter_list_.erase(hdl);
        }
      } else {
        break;
      }
    }
  }
}
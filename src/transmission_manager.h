#ifndef _TRANSIMISSION_MANAGER_H_
#define _TRANSIMISSION_MANAGER_H_

#include <map>
#include <set>
#include <websocketpp/server.hpp>

class TransmissionManager {
 public:
  TransmissionManager();
  ~TransmissionManager();

 public:
  std::vector<std::string> GetAllUserIdOfTransmission(
      const std::string& transmission_id);

 public:
  bool BindHostIdToTransmission(const std::string& host_id,
                                const std::string& transmission_id);
  bool BindGuestIdToTransmission(const std::string& guest_id,
                                 const std::string& transmission_id);
  bool BindPasswordToTransmission(const std::string& password,
                                  const std::string& transmission_id);
  bool BindUserIdToWsHandle(const std::string& user_id,
                            websocketpp::connection_hdl hdl);

  bool IsHostOfTransmission(const std::string& user_id,
                            const std::string& transmission_id);

  std::string ReleaseHostIdFromTransmission(const std::string& host_id);
  std::string ReleaseGuestIdFromTransmission(const std::string& guest_id);

  std::string ReleaseUserIdToWsHandle(websocketpp::connection_hdl hdl);
  bool ReleaseAllUserIdFromTransmission(const std::string& transmission_id);
  bool ReleasePasswordFromTransmission(const std::string& transmission_id);

  websocketpp::connection_hdl GetWsHandle(const std::string& user_id);
  std::string GetUserId(websocketpp::connection_hdl hdl);
  int CheckPassword(const std::string& password,
                    const std::string& transmission_id);
  std::string GetPassword(const std::string& transmission_id);

 private:
  std::map<std::string, std::string> transmission_host_id_list_;
  std::map<std::string, std::vector<std::string>> transmission_guest_id_list_;
  std::map<std::string, std::string> transmission_password_list_;
  std::map<std::string, websocketpp::connection_hdl> user_id_ws_hdl_list_;
};

#endif
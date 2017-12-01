#ifndef GATEWAY_MANAGER_H_
#define GATEWAY_MANAGER_H_

#include "uv.h"
#include <unordered_map>
#include <memory>

class ServerConnection;

class GatewayManager
{
private:
  GatewayManager();
  ~GatewayManager();
public:
  static GatewayManager& GetInstance();
public:
  int OnTimerLong();
  int OnTimerMiddle();
  int OnTimerShort();

public:
  int RegisterServerConncetion(const std::shared_ptr<ServerConnection>& server);
  int UnRegisterServerConncetion(uint32_t serverId);

  std::shared_ptr<ServerConnection> GetServerConnectionById(uint32_t serverId);

  void Clean();
private:
  std::unordered_map<uint32_t, std::shared_ptr<ServerConnection>> m_gwConn;
};

#endif  //! #ifndef GATEWAY_MANAGER_H_

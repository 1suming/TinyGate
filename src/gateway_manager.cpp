#include "gateway_manager.h"
#include "server_connection.h"
#include "spdlog/spdlog++.h"
#include "gateway_accept.h"

GatewayManager::GatewayManager()
{
  
}

GatewayManager::~GatewayManager()
{

}

GatewayManager& GatewayManager::GetInstance()
{
  static GatewayManager inst;
  return inst;
}

int GatewayManager::OnTimerLong()
{
  return 0;
}

int GatewayManager::OnTimerMiddle()
{
  for (auto it = m_gwConn.begin(); it != m_gwConn.end(); ++it)
    it->second->CheckTimeoutExceed();

  return 0;
}

int GatewayManager::OnTimerShort()
{
  return 0;
}

std::shared_ptr<ServerConnection> GatewayManager::GetServerConnectionById(uint32_t serverId)
{
  auto it = m_gwConn.find(serverId);
  if (it == m_gwConn.end())
  {
    LOG(ERROR) << "GetServerConnectionById: serverId. " << serverId << " not exist.";
    return nullptr;
  }

  return it->second;
}

int GatewayManager::RegisterServerConncetion(const std::shared_ptr<ServerConnection>& server)
{
  uint32_t serverId = server->GetServerId();
  if (m_gwConn.find(serverId) != m_gwConn.end())
  {
    LOG(ERROR) << "RegisterServerConncetion: serverId. " << serverId << " already registered.";
    return -1;
  }

  m_gwConn[serverId] = server;
  return 0;
}

int GatewayManager::UnRegisterServerConncetion(uint32_t serverId)
{
  auto it = m_gwConn.find(serverId);
  if (it == m_gwConn.end())
    return -1;

  m_gwConn.erase(it);
  return 0;
}

void GatewayManager::Clean()
{
  for (auto it = m_gwConn.begin(); it != m_gwConn.end(); ++it)
  {
    GatewayAccept::GetInstance().AddToStash(it->second);  //close是异步过程，这里要保持生命周期
    it->second->Close(CR_FORCE_CLEAN);
  }
  m_gwConn.clear();
}

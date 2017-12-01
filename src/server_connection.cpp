#include "server_connection.h"
#include "spdlog/spdlog++.h"
#include "util/base_util.h"
#include "gateway_manager.h"
#include "config.h"
#include <assert.h>
#include "proto/proto_common.pb.h"
#include "client_connection.h"
#include "gateway_accept.h"
#include "proto/proto_server.pb.h"
#include "client_accept.h"

ServerConnection::ServerConnection()
{
  m_serverId = -1;

  GetTimeMillSecond((int64_t*)&m_serverBreathTimestamp);

  memset(m_procFuncMap, 0, sizeof(m_procFuncMap));
  m_procFuncMap[TOGATEWAY_START_SERVER] = &ServerConnection::ProcessS2GStartServer;
  m_procFuncMap[TOGATEWAY_BREATHE_PING] = &ServerConnection::ProcessS2GBreathePing;
}

ServerConnection::~ServerConnection()
{
}

int ServerConnection::ProcessS2GStartServer(const char* data, int32_t len)
{
  QueuedMsg msg;
  if (!msg.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessS2GStartServer: Gateway recv msg from game server parse protocol buffers failed.";
    return -1;
  }

  s2g_start_server start;
  if (!start.ParseFromArray(msg.data().c_str(), msg.data().length()))
  {
    LOG(ERROR) << "ProcessS2GStartServer: Gateway recv msg from game server parse protocol buffers failed.";
    return -1;
  }

  m_serverId = start.server_id();

  LOG(INFO) << "ProcessS2GStartServer: serverid. " << m_serverId;

  //必须先register再remove，不然会得到一个销毁的对象
  if (GatewayManager::GetInstance().RegisterServerConncetion(shared_from_this()) == -1)
  {
    LOG(ERROR) << "ProcessS2GStartServer: registerServerConnection fail. serverid = " << m_serverId;
    Close(CR_FORCE_CLEAN);
    return -1;
  }
  GatewayAccept::GetInstance().RemoveFromStash(shared_from_this());

  GetTimeMillSecond((int64_t*)&m_serverBreathTimestamp);
  return 0;
}

int ServerConnection::ProcessS2GBreathePing(const char* data, int32_t len)
{
  QueuedMsg msg;
  if (!msg.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessS2GBreathePing: Gateway recv msg from game server parse protocol buffers failed.";
    return -1;
  }

  s2g_breathe_ping ping;
  if (!ping.ParseFromArray(msg.data().c_str(), msg.data().length()))
  {
    LOG(ERROR) << "ProcessS2GBreathePing: Gateway recv msg from game server parse protocol buffers failed.";
    return -1;
  }

  GetTimeMillSecond((int64_t*)&m_serverBreathTimestamp);

  g2s_breathe_pong pong;
  pong.set_server_ts(ping.server_ts());
  pong.set_id(ping.id());

  if (ping.server_ping() > 100)
    LOG(ERROR) << "recv server ping: " << ping.server_ping();

  std::string dataStr = pong.SerializeAsString();
  return Send(TOSERVER_BREATHE_PONG, dataStr.c_str(), dataStr.length());
}

int ServerConnection::ProcessS2CMessage(uint32_t msgType, const char* data, int32_t len)
{
  QueuedMsg msg;
  if (!msg.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessS2CMessage: Gateway recv msg from game server parse protocol buffers failed.";
    return 0; //server不应该断开连接，所以这里返回0， 下同
  }

  int64_t uid = msg.uid();
  auto it = m_clients.find(uid);
  if (it == m_clients.end())
  {
    LOG(ERROR) << "ProcessS2CMessage: uid. " << uid << " not exist.";
    return 0;
  }

  const std::string& body = msg.data();
  if (it->second->ProcessS2CMessage(msgType, body.c_str(), body.length()) < 0) //透传协议
  {
    LOG(ERROR) << "ProcessS2CMessage: msgType." << msgType << " uid. " << it->second->getUid() << " send fail.";
  }

  return 0;
}

int ServerConnection::OnRecvPacket(uint32_t msgType, const char* data, int32_t len)
{
  if (msgType < CORE_MAX_NUM_MSGTYPES && m_procFuncMap[msgType])
  {
    ProcFun func = m_procFuncMap[msgType];
    return (this->*func)(data, len);
  }

  ProcessS2CMessage(msgType, data, len);
  return 0;
}

int ServerConnection::OnRecvPacket(const std::string& msgType, const char* data, int32_t len)
{
  LOG(ERROR) << "unknown msg_type: " << msgType;
  return CR_INVALID_PACKET;
}

int ServerConnection::OnRequireClose(int reason)
{
  if (m_connectStatu == CS_CLOSED)
    return -1;

  LOG(INFO) << "OnRequireClose: serverId. " << m_serverId;
  return 0;
}

int ServerConnection::OnAfterClose()
{
  std::map<int64_t, std::shared_ptr<ClientConnection>> clients = m_clients;
  for (auto it = clients.begin(); it != clients.end(); ++it)
  {
    ClientAccept::GetInstance().AddToStash(it->second);   //close是异步过程，这里要保持生命周期
    it->second->Close(CR_FORCE_CLEAN);
  }
  clients.clear();

  if (GatewayAccept::GetInstance().RemoveFromStash(shared_from_this()) == -1)
    GatewayManager::GetInstance().UnRegisterServerConncetion(m_serverId);
    
  return 0;
}

int ServerConnection::RegisterClientConnection(const std::shared_ptr<ClientConnection>& client)
{
  uint64_t uid = client->getUid();
  auto it = m_clients.find(uid);
  if (it == m_clients.end())
  {
    m_clients[uid] = client;
    return 0;
  }

  LOG(WARNING) << "same uid client login, uid:" << client->getUid();
  // 踢掉老客户端
  it->second->denyAccessToClient(SERVER_ACCESSDENIED_CLIENT_EXISTS);
  ClientAccept::GetInstance().AddToStash(it->second);   //close是异步过程，这里要保持生命周期
  it->second->Close(CR_FORCE_CLEAN);

  m_clients[uid] = client;
  return 0;
}

int ServerConnection::UnRegisterClientConnection(int64_t uid)
{
  auto it = m_clients.find(uid);
  if (it == m_clients.end())
    return -1;

  m_clients.erase(it);
  return 0;
}

int ServerConnection::CheckTimeoutExceed()
{
  for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
    it->second->CheckTimeoutExceed();

  uint64_t timeNow = 0;
  GetTimeMillSecond((int64_t*)&timeNow);
  if (timeNow - m_serverBreathTimestamp < SERVER_DIE_TIMEOUT)
    return 0;

  LOG(ERROR) << "SERVER_DIE_TIMEOUT " << m_serverId;
  Close(CR_FORCE_CLEAN);
  return -1;
}

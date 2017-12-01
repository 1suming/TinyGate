#include "client_connection.h"
#include "spdlog/spdlog++.h"
#include "client_connection_queue.h"
#include "util/base_util.h"
#include "config.h"
#include "gateway_manager.h"
#include "proto/proto_client.pb.h"
#include "network/networkprotocol.h"
#include "proto/proto_server.pb.h"
#include "server_connection.h"
#include "client_accept.h"
#include "connection_base.h"
#include "gateway_profiler.h"

ClientConnection::ClientConnection()
{
  m_Uid = -1;
  m_Status = GW_CLIENT_STATUS_INIT;
  m_CurrWorldId = -1;
  m_IsLogin = false;

  GetTimeMillSecond((int64_t*)&m_clientBreathTimestamp);
  memset(m_procFuncMap, 0, sizeof(m_procFuncMap));
  m_procFuncMap[TOGATEWAY_BREATHE_PING] = &ClientConnection::ProcessC2GBreathePing;
  m_procFuncMap[TOSERVER_LOGIN] = &ClientConnection::ProcessC2SLogin;
  m_procFuncMap[TOSERVER_LOGOUT] = &ClientConnection::ProcessC2SLogout;
  m_procFuncMap[TOGATEWAY_CLIENT_KCP_ACK] = &ClientConnection::ProcessC2GKcpAck;
  m_procFuncMap[TOGATEWAY_UDP_CONTROL] = &ClientConnection::ProcessC2GUdpControl;
}

ClientConnection::~ClientConnection()
{
}

int ClientConnection::askServerToLogout()
{
  c2s_logout logout;
  logout.set_param(7777);
  std::string strdata = logout.SerializeAsString();
  packDataAndSendToServer(TOSERVER_LOGOUT, strdata.c_str(), strdata.length());
  LOG(TRACE) << "askServerToLogout: uid." << m_Uid;
  return 0;
}

int ClientConnection::ProcessC2GBreathePing(const char* data, int32_t len)
{
  c2g_breath_ping ping;
  if (!ping.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessC2GBreathe: Parsing from msg failed.";
    return -1;
  }

  GetTimeMillSecond((int64_t*)&m_clientBreathTimestamp);

  g2c_breathe_pong pong;
  pong.set_client_ts(ping.client_ts());
  pong.set_id(ping.id());

  if (ping.client_ping() > 100)
    LOG(ERROR) << "recv client ping: " << ping.client_ping() << " uid. " << m_Uid;

  GatewayProfiler::GetInstance().TraceDelay(m_Uid, CONNECT_TYPE::TCP, m_clientBreathTimestamp, ping.client_ping());
  std::string dataStr = pong.SerializeAsString();
  return Send(TOCLIENT_BREATHE_PONG, dataStr.c_str(), dataStr.length());
}

int ClientConnection::ProcessC2SLogin(const char* data, int32_t len)
{
  if (m_IsLogin)
  {
    LOG(ERROR) << "ProcessC2SLogin: m_IsLogin is already true.";
    return 0;
  }

  c2s_login login;
  if (!login.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessC2SLogin: Parsing from msg failed.";
    return -1;
  }

  m_Uid = login.uid();
  m_CurrWorldId = login.world_id();

  m_server = GatewayManager::GetInstance().GetServerConnectionById(m_CurrWorldId);
  if (!m_server.lock())
  {
    LOG(ERROR) << "ProcessC2SLogin: checkServerStatusByid fail. serverid. " << m_CurrWorldId;
    denyAccessToClient(SERVER_ACCESSDENIED_GAMESERVER_INACTIVE);
    return -1;
  }

  //必须先register再remove，不然会得到一个销毁的对象
  if (-1 == m_server.lock()->RegisterClientConnection(shared_from_this()))
  {
    LOG(ERROR) << "ProcessC2SLogin: updateClientConnection fail. uid. " << m_Uid;
    denyAccessToClient(SERVER_ACCESSDENIED_CLIENT_EXISTS);
    return -1;
  }

  LOG(INFO) << "ProcessC2SLogin: user first login, send init legacy msg to game, uid." << m_Uid;
  packDataAndSendToServer(TOSERVER_LOGIN, data, len);

  ClientAccept::GetInstance().RemoveFromStash(shared_from_this());
  m_IsLogin = true;

  GetTimeMillSecond((int64_t*)&m_clientBreathTimestamp);

  return 0;
}

int ClientConnection::sendKcpConnectKeyToClient(uint32_t kcp_conv, const std::string& peer_ip, int peer_port)
{
  if (m_filter == nullptr)
  {
    LOG(ERROR) << "m_filter == nullptr, uid = " << m_Uid;
    return -1;
  }

  sockaddr_in send_addr;
  uv_ip4_addr(peer_ip.c_str(), peer_port, &send_addr);
  s2c_kcp_key kcp_key;
  kcp_key.set_key(kcp_conv);
  std::string strdata = kcp_key.SerializeAsString();
  GatewayProfiler::GetInstance().TraceSendProtocol(m_Uid, TOCLIENT_KCP_KEY, strdata.length());
  if (Send(TOCLIENT_KCP_KEY, strdata.c_str(), strdata.length()) == -1)
  {
    LOG(ERROR) << "sendKcpConnectKeyToClient fail, uid = " << m_Uid;
    return -1;
  }

  LOG(INFO) << "ProcessC2SLogin: Ask client kcp connect key, uid=" << m_Uid << " kcp_conv=" << m_filter->GetKcpFd()->conv;
  return 0;
}

int ClientConnection::ProcessS2CMessage(uint32_t msgType, const char* data, int32_t len)
{
  GatewayProfiler::GetInstance().TraceSendProtocol(m_Uid, msgType, len);
  if (m_filter && m_filter->IsEable() && m_filter->ProcessAndSendPacket(msgType, data, len) == 0)
    return 0;

  return Send(msgType, data, len);
}

int ClientConnection::ProcessC2SLogout(const char* data, int32_t len)
{
  if (!m_IsLogin)
  {
    LOG(ERROR) << "ProcessC2SLogout: client first protocol is not LOGIN, remove it.";
    return -1;
  }

  LOG(INFO) << "recv logout request, uid." << m_Uid;
  m_Status = GW_CLIENT_STATUS_LOGOUT;
  return -1;
}

int ClientConnection::ProcessC2GKcpAck(const char* data, int32_t len)
{
  if (!m_IsLogin)
  {
    LOG(ERROR) << "ProcessC2GKcpAck: client first protocol is not LOGIN, remove it.";
    return -1;
  }

  c2g_kcp_ack ack;
  if (!ack.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessC2GKcpAck: Parsing from msg failed.";
    return -1;
  }

  LOG(INFO) << "Client : " << m_Uid << " kcp conv " << ack.conv();
  return 0;
}

int ClientConnection::ProcessC2GUdpControl(const char* data, int32_t len)
{
  if (!m_IsLogin)
  {
    LOG(ERROR) << "ProcessC2GUdpControl: client first protocol is not LOGIN, remove it.";
    return -1;
  }

  c2g_udp_control control;
  if (!control.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessC2GUdpControl: Parsing from msg failed.";
    return -1;
  }
  LOG(INFO) << "Client : " << m_Uid << " kcp protocol enable. " << control.enable();

  if (!control.enable() && m_filter)
  {
    m_filter->SetEnable(false);
  }
  else if (control.enable() && m_filter)
  {
    m_filter->SetEnable(true);
  }
  else if (control.enable() && !m_filter)
  {
    m_filter = std::make_shared<KcpPacketFilter>(shared_from_this());
    if (sendKcpConnectKeyToClient(m_filter->GetKcpFd()->conv, GetPeerIp(), GetPeerPort()) == 0)
      UdpClientAcceptCenter::GetInstance().AddVirtualClientConnection(m_filter);
    m_filter->SetEnable(true);
  }

  return 0;
}

int ClientConnection::ProcessC2SMessage(uint32_t msgType, const char* data, int32_t len)
{
  if (!m_IsLogin)
  {
    LOG(ERROR) << "ProcessC2SLogout: client first protocol is not LOGIN, remove it.";
    return -1;
  }

  //LOG(INFO) << "recv client msg type = " << msgType << " size = " << len;
  return packDataAndSendToServer(msgType, data, len);
}

int ClientConnection::packDataAndSendToServer(uint32_t msgType, const char* data, int32_t len)
{
  if (!m_server.lock())
  {
    LOG(ERROR) << "drop msg for worldId=" << m_CurrWorldId << ",uid=" << m_Uid;
    return -1;
  }

  QueuedMsg qmsg;
  qmsg.set_uid(m_Uid);
  qmsg.set_type(msgType);
  qmsg.set_data(data, len);

  std::string dataStr = qmsg.SerializeAsString();
  int rc = m_server.lock()->Send(msgType, dataStr.c_str(), dataStr.length());
  if (rc == -1)
    LOG(ERROR) << "drop msg for worldId=" << m_CurrWorldId << ",uid=" << m_Uid;

  return rc;
}

int ClientConnection::denyAccessToClient(AccessDeniedCode reason)
{
  s2c_access_denied deny;
  deny.set_code((uint32_t)reason);
  std::string str = deny.SerializeAsString();
  GatewayProfiler::GetInstance().TraceSendProtocol(m_Uid, TOCLIENT_ACCESS_DENIED, str.length());
  return Send(TOCLIENT_ACCESS_DENIED, str.c_str(), str.length());
}

int ClientConnection::OnRecvPacket(uint32_t msgType, const char* data, int32_t len)
{
  if (m_Status != GW_CLIENT_STATUS_INIT)
  {
    LOG(INFO) << "client is switching world, drop msg";
    return -1;
  }
  
  GatewayProfiler::GetInstance().TraceRecvProtocol(m_Uid, msgType, len);

  if (msgType < CORE_MAX_NUM_MSGTYPES && m_procFuncMap[msgType])
  {
    ProcFun func = m_procFuncMap[msgType];
    return (this->*func)(data, len);
  }

  return ProcessC2SMessage(msgType, data, len);
}

int ClientConnection::OnRecvPacket(const std::string& msgType, const char* data, int32_t len)
{
  LOG(ERROR) << "unknown msg_type: " << msgType;
  return CR_INVALID_PACKET;
}

int ClientConnection::OnRequireClose(int reason)
{
  if (m_connectStatu == CS_CLOSED)
    return -1;

  LOG(INFO) << "client RequireClose of id: " << m_Uid;
  askServerToLogout();

  return 0;
}

int ClientConnection::OnAfterClose()
{
  UdpClientAcceptCenter::GetInstance().RemoveVirtualClientConnection(m_filter);

  if (ClientAccept::GetInstance().RemoveFromStash(shared_from_this()) == -1 && m_server.lock())
    m_server.lock()->UnRegisterClientConnection(m_Uid);

  GatewayProfiler::GetInstance().TraceUserLogout();
  return 0;
}

int ClientConnection::CheckTimeoutExceed()
{
  uint64_t timeNow = 0;
  GetTimeMillSecond((int64_t*)&timeNow);
  if (timeNow - m_clientBreathTimestamp < CLIENT_DIE_TIMEOUT)
    return 0;

  LOG(ERROR) << "CLIENT_DIE_TIMEOUT " << m_Uid;
  Close(CR_FORCE_CLEAN);
  return -1;
}



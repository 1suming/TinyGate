#ifndef CLIENT_CONNECTION_H_
#define CLIENT_CONNECTION_H_

#include "uv.h"
#include <string>
#include "transport/tcp_server_transport.h"
#include "../../gameserver/common/network/networkprotocol.h"
#include <memory>

using namespace orpc;
class ServerConnection;
class KcpPacketFilter;

enum TRANSMIT_TYPE
{
  TRANSMIT_TCP = 1,
  TRANSMIT_UDP,
  TRANSMIT_KCP
};

enum GW_CLIENT_STATUS
{
  GW_CLIENT_STATUS_INIT,       // 初始状态
  GW_CLIENT_STATUS_LOGIN,        // 收到客户端login请求后进入该状态
  GW_CLIENT_STATUS_LOGOUT        // 收到客户端logout请求后进入该状态
};

class ClientConnection : public TcpServerTransport, public std::enable_shared_from_this<ClientConnection>
{
public:
  ClientConnection();
  ~ClientConnection();
public:
  virtual int OnRecvPacket(uint32_t msgType, const char* data, int32_t len) override;
  virtual int OnRecvPacket(const std::string& msgType, const char* data, int32_t len) override;
  virtual int OnRequireClose(int reason) override;
  virtual int OnAfterClose() override;
public:
  int64_t getUid() const { return m_Uid; }
  int64_t getWorldId() const { return m_CurrWorldId; }
  GW_CLIENT_STATUS getStatus()const { return m_Status; }

  int ProcessS2CMessage(uint32_t msgType, const char* data, int32_t len);

  int CheckTimeoutExceed();
  int denyAccessToClient(AccessDeniedCode reason);
private:
  int ProcessC2GBreathePing(const char* data, int32_t len);
  int ProcessC2SLogin(const char* data, int32_t len);
  int ProcessC2SLogout(const char* data, int32_t len);
  int ProcessC2SMessage(uint32_t msgType, const char* data, int32_t len);
  int ProcessC2GUdpControl(const char* data, int32_t len);
  int ProcessC2GKcpAck(const char* data, int32_t len);

  int askServerToLogout();
  int packDataAndSendToServer(uint32_t msgType, const char* data, int32_t len);
  int sendKcpConnectKeyToClient(uint32_t kcp_conv, const std::string& peer_ip, int peer_port);
public:
  static const int MAX_CLIENT_IDLE_TIME = 10;
private:
  typedef int (ClientConnection::*ProcFun)(const char* data, int32_t len);
  ProcFun m_procFuncMap[CORE_MAX_NUM_MSGTYPES];

  int m_Uid;
  GW_CLIENT_STATUS m_Status;
  int m_CurrWorldId;
  bool m_IsLogin;
  bool m_IsKcpValid;
  uint64_t m_clientBreathTimestamp;
  std::shared_ptr<KcpPacketFilter> m_filter;
  std::weak_ptr<ServerConnection> m_server;
private:
  static const int CLIENT_DIE_TIMEOUT = 6000;
};

#endif  //! #ifndef CLIENT_CONNECTION_H_

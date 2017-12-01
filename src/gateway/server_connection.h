#ifndef GATEWAY_CONNECTION_H_
#define GATEWAY_CONNECTION_H_

#include "uv.h"
#include "util/thread_safe_list.h"
#include "transport/tcp_server_transport.h"
#include "client_connection_queue.h"
#include "../../gameserver/common/network/networkprotocol.h"
#include <memory>

using namespace orpc;
class ClientConnection;
class QueuedMsg;

class ServerConnection : public TcpServerTransport, public std::enable_shared_from_this<ServerConnection>
{
public:
  ServerConnection();
  ~ServerConnection();

public:
  virtual int OnRecvPacket(uint32_t msgType, const char* data, int32_t len) override;
  virtual int OnRecvPacket(const std::string& msgType, const char* data, int32_t len) override;
  virtual int OnRequireClose(int reason) override;
  virtual int OnAfterClose() override;
public:
  uint32_t GetServerId() const  { return m_serverId; }

  int RegisterClientConnection(const std::shared_ptr<ClientConnection>& client);
  int UnRegisterClientConnection(int64_t uid);

  int CheckTimeoutExceed();
private:
  int ProcessS2GStartServer(const char* data, int32_t len);
  int ProcessS2GBreathePing(const char* data, int32_t len);
  int ProcessS2CMessage(uint32_t msgType, const char* data, int32_t len);
private:
  typedef int (ServerConnection::*ProcFun)(const char* data, int32_t len);
  ProcFun m_procFuncMap[CORE_MAX_NUM_MSGTYPES];

  uint32_t m_serverId; // world id

  uint64_t m_serverBreathTimestamp;

  std::map<int64_t, std::shared_ptr<ClientConnection>> m_clients;
private:
  static const int SERVER_DIE_TIMEOUT = 3000;
};

#endif  //! #ifndef GATEWAY_CONNECTION_H_

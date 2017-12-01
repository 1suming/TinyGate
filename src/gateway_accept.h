#ifndef GATEWAY_ACCEPT_H_
#define GATEWAY_ACCEPT_H_

#include "uv.h"
#include "acceptor/tcp_acceptor.h"
#include "server_connection.h"

using namespace orpc;

class GatewayAccept : public TcpAcceptor
{
public:
  GatewayAccept();
  ~GatewayAccept();

  static GatewayAccept& GetInstance();
public:
  virtual TcpServerTransport* NewTcpServerTransport();

  int UpdateStashTimeout();
  int AddToStash(const std::shared_ptr<ServerConnection>& transport);
  int RemoveFromStash(const std::shared_ptr<ServerConnection>& transport);
private:
  std::list<std::shared_ptr<ServerConnection>> m_serverConnStash;
};

#endif  //! #ifndef GATEWAY_ACCEPT_H_

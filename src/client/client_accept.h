#ifndef CLIENT_ACCEPT_H_
#define CLIENT_ACCEPT_H_

#include "uv.h"
#include "acceptor/tcp_acceptor.h"
#include "client_connection.h"

using namespace orpc;

class ClientAccept : public TcpAcceptor
{
public:
  ClientAccept();
  ~ClientAccept();

  static ClientAccept& GetInstance();
public:
  virtual TcpServerTransport* NewTcpServerTransport();

  int UpdateStashTimeout();
  int AddToStash(const std::shared_ptr<ClientConnection>& transport);
  int RemoveFromStash(const std::shared_ptr<ClientConnection>& transport);
private:
  std::list<std::shared_ptr<ClientConnection>> m_clientConnStash;
};

#endif  //! #ifndef CLIENT_ACCEPT_H_

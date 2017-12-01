#include "client_accept.h"
#include "gateway_profiler.h"

ClientAccept::ClientAccept()
{

}

ClientAccept::~ClientAccept()
{

}

ClientAccept& ClientAccept::GetInstance()
{
  static ClientAccept inst;
  return inst;
}

TcpServerTransport* ClientAccept::NewTcpServerTransport()
{
  std::shared_ptr<ClientConnection> transport = std::make_shared<ClientConnection>();
  m_clientConnStash.push_back(transport);
  GatewayProfiler::GetInstance().TraceUserLogin();
  return transport.get();
}

int ClientAccept::AddToStash(const std::shared_ptr<ClientConnection>& transport)
{
  m_clientConnStash.push_back(transport);
  return 0;
}

int ClientAccept::RemoveFromStash(const std::shared_ptr<ClientConnection>& transport)
{
  auto it = std::find(m_clientConnStash.begin(), m_clientConnStash.end(), transport);
  if (it == m_clientConnStash.end())
    return -1;

  m_clientConnStash.erase(it);
  return 0;
}

int ClientAccept::UpdateStashTimeout()
{
  for (auto it = m_clientConnStash.begin(); it != m_clientConnStash.end(); ++it)
    (*it)->CheckTimeoutExceed();

  return 0;
}
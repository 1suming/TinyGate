#include "gateway_accept.h"
#include "gateway_manager.h"

GatewayAccept::GatewayAccept()
{

}

GatewayAccept::~GatewayAccept()
{

}

GatewayAccept& GatewayAccept::GetInstance()
{
  static GatewayAccept inst;
  return inst;
}

TcpServerTransport* GatewayAccept::NewTcpServerTransport()
{ 
  std::shared_ptr<ServerConnection> transport = std::make_shared<ServerConnection>();
  m_serverConnStash.push_back(transport);
  return transport.get();
}

int GatewayAccept::AddToStash(const std::shared_ptr<ServerConnection>& transport)
{
  m_serverConnStash.push_back(transport);
  return 0;
}

int GatewayAccept::RemoveFromStash(const std::shared_ptr<ServerConnection>& transport)
{
  auto it = std::find(m_serverConnStash.begin(), m_serverConnStash.end(), transport);
  if (it == m_serverConnStash.end())
    return -1;

  m_serverConnStash.erase(it);
  return 0;
}

int GatewayAccept::UpdateStashTimeout()
{
  for (auto it = m_serverConnStash.begin(); it != m_serverConnStash.end(); ++it)
    (*it)->CheckTimeoutExceed();

  return 0;
}

#include "connection_base.h"
#include "client_connection.h"
#include "util/base_util.h"
#include "client_accept.h"
#include "proto/proto_client.pb.h"
#include "proto/proto_server.pb.h"
#include "gateway_profiler.h"

void UdpSender::Release()
{
  if (data)
    free((void*)data);
  delete this;
}

UdpClientAcceptCenter& UdpClientAcceptCenter::GetInstance()
{
  static UdpClientAcceptCenter inst;
  return inst;
}

UdpClientAcceptCenter::UdpClientAcceptCenter()
{
}

UdpClientAcceptCenter::~UdpClientAcceptCenter()
{
}

void UdpClientAcceptCenter::sv_alloc_cb(uv_handle_t* handle,
  size_t suggested_size,
  uv_buf_t* buf)
{
  UdpClientAcceptCenter* peer = (UdpClientAcceptCenter*)handle->data;
  buf->base = peer->m_recvBuf;
  buf->len = MAX_UDP_PACKET_UNIT_LEN;
}

void UdpClientAcceptCenter::sv_recv_cb(uv_udp_t* handle,
  ssize_t nread,
  const uv_buf_t* buf,
  const struct sockaddr* addr,
  unsigned flags)
{
  UdpClientAcceptCenter* peer = (UdpClientAcceptCenter*)handle->data;
  peer->OnRead(handle, nread, buf, addr);
}

void UdpClientAcceptCenter::write_cb(uv_udp_send_t* req, int status)
{
  if (status != 0)
    LOG(ERROR) << "[UdpClientAcceptCenter::write_cb] error " << uv_strerror(status) << std::endl;

  UdpSender* data = (UdpSender*)req;
  data->Release();
}

int UdpClientAcceptCenter::Init(const std::string& ip, int port, uv_loop_t* loop /* = 0 */)
{
  int r;
  r = uv_ip4_addr(ip.c_str(), port, &m_listenAddr);
  UV_CHECK_RET_1(uv_ip4_addr, r);

  m_udp.data = (void*)this;
  r = uv_udp_init(loop ? loop : uv_default_loop(), (uv_udp_t*)&m_udp);
  UV_CHECK_RET_1(uv_tcp_init, r);

  r = uv_udp_bind((uv_udp_t*)&m_udp, (const struct sockaddr*)&m_listenAddr, 0);
  UV_CHECK_RET_1(uv_tcp_bind, r);

  return 0;
}

int UdpClientAcceptCenter::UnInit()
{
  uv_udp_recv_stop(&m_udp);
  uv_close((uv_handle_t*)&m_udp, 0);
  return 0;
}

int UdpClientAcceptCenter::Listen()
{
  int r = uv_udp_recv_start(&m_udp, sv_alloc_cb, sv_recv_cb);
  UV_CHECK_RET_1(uv_udp_recv_start, r);
  return 0;
}

int UdpClientAcceptCenter::Send(const char* data, size_t len, const char* ip, int32_t port)
{
  int r;

  char* cdata = (char*)malloc(len);
  memcpy(cdata, data, len);
  m_sendReq = new UdpSender(this, cdata);
  sockaddr_in send_addr;
  r = uv_ip4_addr(ip, port, &send_addr);
  uv_buf_t buf = uv_buf_init((char*)cdata, len);
  r = uv_udp_send((uv_udp_send_t*)m_sendReq,
    &m_udp,
    &buf,
    1,
    (const struct sockaddr*)&send_addr,
    write_cb);
  if (r < 0)
  {
    m_sendReq->Release();
    ORPC_LOG(ERROR) << "[ORPC::Send] uv_write error: " << uv_err_name(r) << std::endl;
    return r;
  }

  return 0;
}

void UdpClientAcceptCenter::OnTimerShort()
{
  for (auto& it : m_dummy_conn_map)
    it.second->Update();
}

int UdpClientAcceptCenter::OnRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr)
{
  if (nread < 0)
  {
    LOG(ERROR) << "[UdpClientAcceptCenter::OnRead] EOF read on udp datagram: " << uv_strerror(nread) << std::endl;
    return -1;
  }
  else if (nread == 0)
    return 0;
  IUINT32 conv;
  conv = ikcp_getconv(buf->base);
  auto it = m_dummy_conn_map.find(conv);
  if (it == m_dummy_conn_map.end())
  {
    LOG(ERROR) << "UdpClientAcceptCenter： Unregistered ikcp conv:" << conv;
    return -1;
  }

  return it->second->OnRecvPacket(buf->base, nread, addr);
}

int UdpClientAcceptCenter::AddVirtualClientConnection(const std::shared_ptr<KcpPacketFilter>& clientConnection)
{
  int conv = clientConnection->GetKcpFd()->conv;
  if (m_dummy_conn_map.find(conv) != m_dummy_conn_map.end())
  {
    LOG(ERROR) << "AddVirtualClientConnection: clientId. " << conv << " already registered.";
    return -1;
  }

  m_dummy_conn_map[conv] = clientConnection;
  return 0;
}

int UdpClientAcceptCenter::RemoveVirtualClientConnection(const std::shared_ptr<KcpPacketFilter>& clientConnection)
{
  if (clientConnection == nullptr)
    return -1;

  auto it = m_dummy_conn_map.find(clientConnection->GetKcpFd()->conv);
  if (it == m_dummy_conn_map.end())
  {
    LOG(ERROR) << "RemoveVirtualClientConnectionByKcpid: couldn't find clientId. " << clientConnection->GetKcpFd()->conv;
    return -1;
  }

  m_dummy_conn_map.erase(it);
  return 0;
}

KcpPacketFilter::KcpPacketFilter(const std::shared_ptr<ClientConnection>& clientConnection)
  : m_clientConnection(clientConnection), m_filterEnable(false)
{
  static IUINT32 s_conv = 1;
  m_kcp = ikcp_create(s_conv++, (void*)this);
  m_kcp->output = kcp_udp_output;
  GetTimeMillSecond(&m_beginTimestamp);
  ikcp_update(m_kcp, 0);
  ikcp_nodelay(m_kcp, 1, 10, 2, 1);
}

KcpPacketFilter::~KcpPacketFilter()
{
  ikcp_release(m_kcp);
}

int KcpPacketFilter::kcp_udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
  KcpPacketFilter* peer = (KcpPacketFilter*)(user);
  return UdpClientAcceptCenter::GetInstance().Send(buf, len, peer->m_peerIp, peer->m_peerPort);
}

int KcpPacketFilter::ProcessAndSendPacket(uint32_t msgType, const char* data, int32_t len)
{
  if (len >= LargePacket::MAX_BODY_LENGTH)
  {
    LOG(ERROR) << "len > LargePacket::MAX_BODY_LENGTH. " << std::endl;
    return -1;
  }
  LargePacket packet;
  packet.setMsgType(msgType);
  uint16_t totalLen = len + Packet::PACKET_HEAD_LENGTH;
  packet.setTotalLength(totalLen);
  packet.setBodyData(data, len);
  int r = ikcp_send(m_kcp, (char*)&packet, packet.getTotalLength());
  ikcp_flush(m_kcp);
  return r;
}

int KcpPacketFilter::OnRecvPacket(const char* data, int32_t len, const struct sockaddr* addr)
{
  int r;
  struct sockaddr_in* sin = (struct sockaddr_in*)addr;
  r = uv_ip4_name(sin, m_peerIp, INET_ADDRSTRLEN);
  m_peerPort = ntohs(sin->sin_port);
  ikcp_input(m_kcp, data, len);
  r = ikcp_recv(m_kcp, m_recvBuffer, MAX_UDP_PACKET_UNIT_LEN);
  if (r < 0) // ikcp_recv返回非零值表示EAGAIN
    return 0;

  uint16_t total_length = ntohs(*(uint16_t*)(m_recvBuffer));
  uint16_t msg_type = ntohs(*(uint16_t*)(m_recvBuffer + sizeof(uint16_t)));
  if (total_length != r)
  {
    LOG(ERROR) << "VerifyRecvPack : recv incompleted packet.";
    return -1;
  }

  if (msg_type == TOGATEWAY_BREATHE_PING)
    return ProcessC2GBreathePing(m_recvBuffer + Packet::PACKET_HEAD_LENGTH, r - Packet::PACKET_HEAD_LENGTH);

  return m_clientConnection->OnRecvPacket(msg_type, m_recvBuffer + Packet::PACKET_HEAD_LENGTH, r - Packet::PACKET_HEAD_LENGTH);
}

int KcpPacketFilter::ProcessC2GBreathePing(const char* data, int32_t len)
{
  c2g_breath_ping ping;
  if (!ping.ParseFromArray(data, len))
  {
    LOG(ERROR) << "ProcessC2GBreathe: Parsing from msg failed.";
    return -1;
  }

  g2c_breathe_pong pong;
  pong.set_client_ts(ping.client_ts());
  pong.set_id(ping.id());

  if (ping.client_ping() > 100)
    LOG(ERROR) << "recv client ping: " << ping.client_ping() << " uid. " << m_clientConnection->getUid();

  uint64_t timeNow;
  GetTimeMillSecond((int64_t*)&timeNow);
  GatewayProfiler::GetInstance().TraceDelay(m_clientConnection->getUid(), CONNECT_TYPE::UDP, timeNow, ping.client_ping());
  std::string dataStr = pong.SerializeAsString();
  return ProcessAndSendPacket(TOCLIENT_BREATHE_PONG, dataStr.c_str(), dataStr.length());
}

void KcpPacketFilter::Update()
{
  int64_t currTime;
  GetTimeMillSecond(&currTime);
  ikcp_update(m_kcp, currTime - m_beginTimestamp);
}

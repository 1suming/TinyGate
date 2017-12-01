#ifndef CONNECTION_BASE_H_
#define CONNECTION_BASE_H_

#include <unordered_map>
#include "uv.h"
#include "acceptor/tcp_acceptor.h"
#include "ikcp.h"

#define MAX_UDP_PACKET_UNIT_LEN 65535

using namespace orpc;

class ClientConnection;

struct UdpSender
{
  uv_udp_send_t req;
  void* conn;
  const char* data;
  UdpSender(void* v, const char* d) : conn(v), data(d) { }
  void Release();
};

class KcpPacketFilter;

class UdpClientAcceptCenter : public IAcceptor
{
private:
  UdpClientAcceptCenter();
  ~UdpClientAcceptCenter();
public:
  static UdpClientAcceptCenter& GetInstance();
public:
  int Init(const std::string& ip, int port, uv_loop_t* loop = 0) override;
  int UnInit() override;
  int Listen() override;
  int Send(const char* data, size_t len, const char* ip, int32_t port);

  void OnTimerShort();
public:
  int OnRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, 
    const struct sockaddr* addr);
public:
  int AddVirtualClientConnection(const std::shared_ptr<KcpPacketFilter>& clientConnection);
  int RemoveVirtualClientConnection(const std::shared_ptr<KcpPacketFilter>& clientConnection);
private:
  static void sv_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
  static void sv_recv_cb(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, 
    const struct sockaddr* addr, unsigned flags);
  static void write_cb(uv_udp_send_t* req, int status);
  static void close_cb(uv_handle_t* handle);
private:
  uv_udp_t m_udp;
  sockaddr_in m_listenAddr;
  UdpSender* m_sendReq;
  char m_recvBuf[MAX_UDP_PACKET_UNIT_LEN];
  // 存储客户端的kcp_conv与客户端实例的映射
  std::unordered_map<uint32_t, std::shared_ptr<KcpPacketFilter>> m_dummy_conn_map;
};

class KcpPacketFilter
{
public:
  KcpPacketFilter(const std::shared_ptr<ClientConnection>& clientConnection);
  ~KcpPacketFilter();
public:
  int ProcessAndSendPacket(uint32_t msgType, const char* data, int32_t len);
  int OnRecvPacket(const char* data, int32_t len, const struct sockaddr* addr);
  ikcpcb* GetKcpFd() { return m_kcp; }
  const char* GetPeerIp() { return m_peerIp; }
  int GetPeerPort() { return m_peerPort; }

  void Update();
  bool IsEable() { return m_filterEnable; }
  void SetEnable(bool val) { m_filterEnable = val; }
private:
  static int kcp_udp_output(const char *buf, int len, ikcpcb *kcp, void *user);
  int ProcessC2GBreathePing(const char* data, int32_t len);
private:
  char m_peerIp[INET_ADDRSTRLEN];
  int m_peerPort;
  ikcpcb* m_kcp;
  char m_recvBuffer[MAX_UDP_PACKET_UNIT_LEN];
  bool m_filterEnable;
  std::shared_ptr<ClientConnection> m_clientConnection;
  int64_t m_beginTimestamp;
};

#endif  //! #ifndef CONNECTION_BASE_H_

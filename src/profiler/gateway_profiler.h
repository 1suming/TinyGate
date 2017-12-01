#ifndef GATEWAY_PROFILER_H
#define GATEWAY_PROFILER_H

#include <map>
#include <list>
#include "stdint.h"

enum CONNECT_TYPE
{
  TCP = 0,
  UDP = 1,
};

struct UserDelayStat
{
  int uid;
  CONNECT_TYPE type;
  uint64_t timestamp;
  uint32_t pingdelay;
};
 
struct ProtoStat
{
  int cmd;
  uint64_t count;
  uint64_t size;
};

struct UserStat
{
  int uid;
  uint64_t recv_pkt;
  uint64_t recv_byte_size;

  uint64_t send_pkt;
  uint64_t send_byte_size;
};

class GatewayProfiler
{
public:
  static GatewayProfiler& GetInstance();
  void TraceRecvProtocol(int uid, int msgType, int size);
  void TraceSendProtocol(int uid, int msgType, int size);
  void TraceUserLogin();
  void TraceUserLogout();
  void TraceDelay(int uid, CONNECT_TYPE type, uint64_t timestamp, uint32_t pingdelay);

  void OnTimerMiddle();
  void OnTimerLogInterval();

  uint64_t GetTotalSendSize() const { return m_sendTotleSize; }
  uint64_t GetTotalRecvSize() const { return m_sendTotleSize; }
private:
  void stepBandWidthStat();
  void stepDelayStat();
  void stepUserStat();
  void stepProtoStat();

  void printProtoStat();
  void printUserStat();
  void printDelayStat();

  GatewayProfiler();
  ~GatewayProfiler();
private:

  uint64_t m_onlineClient;
  std::map<int, ProtoStat> m_protoRecvMap;
  std::map<int, ProtoStat> m_protoSendMap;
  std::map<int, UserStat>  m_userNetMap;
  std::list<UserDelayStat> m_userDelaySet;
  uint64_t m_recvTotleSize;
  uint64_t m_sendTotleSize;
  uint64_t m_maxRecvRate;
  uint64_t m_maxSendRate;

  uint64_t m_lastSendTotleSize;
  uint64_t m_lastRecvTotalSize;
};

#endif  //! #ifndef GATEWAY_PROFILER_H


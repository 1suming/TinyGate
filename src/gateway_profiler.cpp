#include "gateway_profiler.h"
#include "spdlog/spdlog++.h"
 
GatewayProfiler::GatewayProfiler() :
m_sendTotleSize(0), m_recvTotleSize(0), m_maxRecvRate(0),
m_maxSendRate(0), m_lastRecvTotalSize(0), m_lastSendTotleSize(0), m_onlineClient(0)
{

}

GatewayProfiler::~GatewayProfiler()
{

}

GatewayProfiler& GatewayProfiler::GetInstance()
{
  static GatewayProfiler inst;
  return inst;
}

void GatewayProfiler::OnTimerMiddle()
{
  stepDelayStat();
}

void GatewayProfiler::OnTimerLogInterval()
{
  stepProtoStat();
  stepUserStat();
  stepBandWidthStat();
}

void GatewayProfiler::stepBandWidthStat()
{
  int recv_rate = (m_recvTotleSize - m_lastRecvTotalSize) / (5);
  int send_rate = (m_sendTotleSize - m_lastSendTotleSize) / (5);

  if (send_rate > m_maxSendRate)
    m_maxSendRate = send_rate;

  if (recv_rate > m_maxRecvRate)
    m_maxRecvRate = recv_rate;

  m_lastRecvTotalSize = m_recvTotleSize;
  m_lastSendTotleSize = m_sendTotleSize;

  LOG(INFO) << "EVENT:RECVSENDSTAT:recv_rate=" << recv_rate << " max_recv_rate=" << m_maxRecvRate 
    << " send_rate=" << send_rate << " max_send_rate=" << m_maxSendRate
    << " recv_totle_size=" << m_recvTotleSize << " send_totle_size=" << m_sendTotleSize;
}

void GatewayProfiler::stepDelayStat()
{
  printDelayStat();
  m_userDelaySet.clear();
}

void GatewayProfiler::stepProtoStat()
{
  printProtoStat();
  m_protoRecvMap.clear();
  m_protoSendMap.clear();
}

void GatewayProfiler::stepUserStat()
{
  printUserStat();
  m_userNetMap.clear();
}

void GatewayProfiler::TraceRecvProtocol(int uid, int type, int size)
{
  m_recvTotleSize += size;
  auto it = m_protoRecvMap.insert(std::pair<int, ProtoStat >(type, { type, 0, 0 }));
  ProtoStat& proto = it.first->second;
  proto.count++;
  proto.size += size;

  auto it2 = m_userNetMap.insert(std::pair<int, UserStat >(uid, { uid, 0, 0, 0, 0 }));
  UserStat& user = it2.first->second;

  user.recv_pkt++;
  user.recv_byte_size += size;
}

void GatewayProfiler::TraceSendProtocol(int uid, int type, int size)
{
  m_sendTotleSize += size;
  auto it = m_protoSendMap.insert(std::pair<int, ProtoStat >(type, { type, 0, 0 }));
  ProtoStat& proto = it.first->second;
  proto.count++;
  proto.size += size;

  auto it2 = m_userNetMap.insert(std::pair<int, UserStat >(uid, { uid, 0, 0, 0, 0 }));
  UserStat& user = it2.first->second;
  user.send_pkt++;
  user.send_byte_size += size;
}

void GatewayProfiler::TraceDelay(int uid, CONNECT_TYPE type, uint64_t timestamp, uint32_t pingdelay)
{
  m_userDelaySet.push_back(UserDelayStat{ uid, type, timestamp, pingdelay });
}

void GatewayProfiler::TraceUserLogin()
{
  ++m_onlineClient;
}

void GatewayProfiler::TraceUserLogout()
{
  --m_onlineClient;
}
 
void GatewayProfiler::printProtoStat()
{
  for (auto it : m_protoRecvMap)
  {
    LOG(INFO) << "EVENT:PROTORECVSTAT:msgtype=" << it.second.cmd << " count=" << it.second.count << " totalsize=" << it.second.size;
  }
  for (auto it : m_protoSendMap)
  {
    LOG(INFO) << "EVENT:PROTOSENDSTAT:msgtype=" << it.second.cmd << " count=" << it.second.count << " totalsize=" << it.second.size;
  }
}

void GatewayProfiler::printUserStat()
{
  for (auto it : m_userNetMap)
  {
    LOG(INFO) << "EVENT:CLIENTSTAT:uid=" << it.second.uid << " recv=" << it.second.recv_pkt << " recvsize=" << it.second.recv_byte_size << " send=" << it.second.send_pkt << " sendsize=" << it.second.send_byte_size;
  }
  LOG(INFO) << "EVENT:CLIENTCOUNT:num=" << m_onlineClient;
}

void GatewayProfiler::printDelayStat()
{

  static const char type_str[2][4] = { "TCP", "UDP" };
  for (auto it : m_userDelaySet)
  {
    LOG(INFO) << "EVENT:DELAYSTAT:uid:" << it.uid << " net:" << type_str[it.type] << " timestamp:" << it.timestamp << " pingdelay:" << it.pingdelay;
  }
}

#include "client_connection_queue.h"
#include "client_connection.h"
#include "spdlog/spdlog++.h"
#include "config.h"
#include <algorithm>
#include <iostream>
#include "util/base_util.h"

ClientConnectionQueue::ClientConnectionQueue()
{
  m_maxNum = 0;
  m_currentNum = 0;
  m_startQueueTime = -1;
  m_totalPassNum = 0;
}

ClientConnectionQueue::~ClientConnectionQueue()
{
  if (!m_connQueue.empty())
    LOG(ERROR) << "m_connQueue is not empty when ~! ";

  if (!m_passClient.empty())
    LOG(ERROR) << "m_passClient is not empty when ~! ";
}

int ClientConnectionQueue::CheckSessionTimeout()
{
  std::map<int64_t, ClientConncetion*> copy = m_passClient;
  auto it = copy.begin();
  for (; it != copy.end(); ++it)
    it->second->CheckSessionTimeout();

  return 0;
}

int ClientConnectionQueue::QueueConncetion(ClientConncetion* client)
{
  if (client == nullptr)
    return -1;

  m_connQueue.push_back(client);
  PassClientAsManyAsPossible();
  return 0;
}

int ClientConnectionQueue::UnQueueConncetion(ClientConncetion* client)
{
  if (!client)
    return -1;

  uint64_t uid = client->getUid();
  auto it = m_passClient.find(uid);
  if (it != m_passClient.end() && it->second == client)
  {
    m_passClient.erase(it);
    return 0;
  }

  auto it2 = std::find(m_connQueue.begin(), m_connQueue.end(), client);
  if (it2 != m_connQueue.end())
  {
    m_connQueue.erase(it2);
    return 0;
  }

  return -1;
}

size_t ClientConnectionQueue::PassClientAsManyAsPossible()
{
  if (m_passClient.size() + m_currentNum >= m_maxNum)
    return 0;

  size_t n = m_maxNum - m_currentNum - m_passClient.size();
  size_t i = 0;
  for (auto it = m_connQueue.begin(); it != m_connQueue.end() && i < n; ++i)
  {
    ClientConncetion* conn = *it;
    int r = DoPassClient(conn);
    if (r != 0)
      conn->Close(r);
    it = m_connQueue.erase(it);
  }

  return i;
}

int ClientConnectionQueue::UpdateStatusToClient(bool gatewayok, int64_t curTime)
{
  int tps = 1;
  if (m_startQueueTime > 0 && (curTime - m_startQueueTime) > 1)
    tps = m_totalPassNum / (curTime - m_startQueueTime);

  int totalCount = m_connQueue.size();
  int queuePos = 0;
  for (auto it = m_connQueue.begin(); it != m_connQueue.end(); ++it)
  {
    (*it)->UpdateStatusToClient(queuePos, totalCount, tps, gatewayok, curTime);
    ++queuePos;
  }

  return 0;
}

bool ClientConnectionQueue::IsValidSession(int64_t uid, int64_t session)
{
  std::map<int64_t, ClientConncetion*>::iterator it = m_passClient.find(uid);
  if (it == m_passClient.end())
  {
    LOG(ERROR) << "session not exist! uid: " << uid << " session_req: " << session;
    return false;
  }

  ClientConncetion* client = it->second;
  bool valid = client->getQueueSession() == session;
  if (!valid)
  {
    LOG(ERROR) << "invalid session! uid: " << uid << " session: " << client->getQueueSession() << " session_req: " << session;
  }
  client->Close(CCR_SESSION_QUERYED);
  return valid;
}

void ClientConnectionQueue::Clean()
{
  m_maxNum = 0;
  m_currentNum = 0;
  m_startQueueTime = -1;
  m_totalPassNum = 0;

  std::list<ClientConncetion*> copy = m_connQueue;
  for (auto it = copy.begin(); it != copy.end(); ++it)
    (*it)->Close(CR_FORCE_CLEAN);

  std::map<int64_t, ClientConncetion*> copyC = m_passClient;
  for (auto it = copyC.begin(); it != copyC.end(); ++it)
    it->second->Close(CR_FORCE_CLEAN);
}

int ClientConnectionQueue::PrintStatus()
{
  std::cout << "ClientConnectionQueue Statu:" << std::endl;
  std::cout << "cur num:" << m_currentNum << " max num: " << m_maxNum << std::endl;
  std::cout << "q time:" << m_startQueueTime << " total num: " << m_totalPassNum << std::endl;

  std::cout << "queue count:" << m_connQueue.size() << std::endl;
  for (auto it = m_connQueue.begin(); it != m_connQueue.end(); ++it)
    std::cout << "queue client id:" << (*it)->getUid() << std::endl;

  std::cout << "pass count:" << m_passClient.size() << std::endl;
  for (auto it = m_passClient.begin(); it != m_passClient.end(); ++it)
    std::cout << "pass client id:" << it->first << std::endl;

  return 0;
}

int ClientConnectionQueue::UpdateNum(int currentNum, int maxNum)
{
  m_maxNum = maxNum;
  m_currentNum = currentNum;
  LOG(INFO) << "UpdateNum, currentNum: " << m_currentNum << " maxNum: " << maxNum;
  PassClientAsManyAsPossible();
  return 0;
}

int ClientConnectionQueue::DoPassClient(ClientConncetion* client)
{
  if (m_connQueue.size() + m_passClient.size() + m_currentNum< m_maxNum)
  {
    m_startQueueTime = -1;
    m_totalPassNum = 0;
  }
  else // queueing
  {
    if (m_startQueueTime < 0)
      GetTimeSecond(&m_startQueueTime);
    m_totalPassNum += 1;
  }

  int64_t uid = client->getUid();

  auto it = m_passClient.find(uid);
  if (it != m_passClient.end())
  {
    LOG(ERROR) << "uid is dup: " << uid;
    return CCR_CLIENT_ID_DUP;
  }

  int r = client->PassToClient();
  if (r < 0)
  {
    LOG(ERROR) << "PassToClient return < 0: " << uid;
    return r;
  }

  m_passClient[uid] = client;
  return 0;
}

#ifndef CLIENT_CONNECTION_QUEUE_H_
#define CLIENT_CONNECTION_QUEUE_H_

#include "uv.h"
#include <list>
#include <map>

class ClientConncetion;
class QueueScheduler;

class ClientConnectionQueue
{
public:
  ClientConnectionQueue();
  ~ClientConnectionQueue();
public:
  int QueueConncetion(ClientConncetion* client);
  int UnQueueConncetion(ClientConncetion* client);

  int CheckSessionTimeout();
  int UpdateStatusToClient(bool gatewayok, int64_t curTime);

  int UpdateNum(int currentNum, int maxNum);
  bool IsValidSession(int64_t uid, int64_t session);

  void Clean();
  int PrintStatus();
private:
  size_t PassClientAsManyAsPossible();
  int DoPassClient(ClientConncetion* client);
private:
  size_t m_maxNum;
  size_t m_currentNum;

  int64_t m_startQueueTime; // 开始排队时间
  int m_totalPassNum;   // 开始排队以来，累计放行人数

  std::list<ClientConncetion*> m_connQueue;

  std::map<int64_t, ClientConncetion*> m_passClient;
};

#endif  //! #ifndef CLIENT_CONNECTION_QUEUE_H_

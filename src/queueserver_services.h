#ifndef QUEUESERVER_SERVICE_H_
#define QUEUESERVER_SERVICE_H_

#include "rpc_application.h"
#include "queueserver_api.h"

using namespace orpc;

class QueueServerApp : public RpcServerApplication
{
public:
  QueueServerApp(const char* cfgFile);
private:
  int Init(const char* cfgFile);
  int UnInit();
};

#endif  //! #ifndef QUEUESERVER_SERVICE_H_

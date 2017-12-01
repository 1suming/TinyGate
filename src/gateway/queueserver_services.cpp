#include "queueserver_services.h"
#include "spdlog/spdlog++.h"
#include <iostream>

QueueServerApp::QueueServerApp(const char* cfgFile)
{
  bool bInited = RpcServerApplication::Init(cfgFile);
  if (!bInited)
    throw std::runtime_error("failed to init rpc.");

  Init(cfgFile);
}

int QueueServerApp::Init(const char* cfgFile)
{
  return 0;
}

int QueueServerApp::UnInit()
{
  return 0;
}


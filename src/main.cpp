#ifndef _MAIN_H_
#define _MAIN_H_

#include "uv.h"
#include "spdlog/spdlog++.h"
#include "client_accept.h"
#include "gateway_accept.h"
#include "util/base_util.h"
#include <assert.h>
#include "gateway_manager.h"
#include "connection_base.h"
#include <thread>
#include <fstream>
#include "../../gameserver/common/version.h"
#include "gateway_profiler.h"
 
YLogger * defaultLogger = nullptr;

uv_timer_t timer_short;
uv_timer_t timer_long;
uv_timer_t timer_middle;
uv_timer_t timer_loginterval;

int long_timer_interval;
int short_timer_interval;
int middle_timer_interval;
int log_timer_interval;

static std::string cfg = "./gateway.conf";
std::map<std::string, std::string> g_config;

int accept_client(std::string _ip, int _port)
{
	LOG(INFO) << "TCP : listen client ip = " << _ip << " port = " << _port;
	ClientAccept::GetInstance().Init( _ip, _port);
  ClientAccept::GetInstance().Listen();
  LOG(INFO) << "UDP : listen client ip = " << _ip << " port = " << _port;
  UdpClientAcceptCenter::GetInstance().Init(_ip, _port);
  UdpClientAcceptCenter::GetInstance().Listen();
  return 0;
}

int accept_server(std::string _ip, int _port)
{
	LOG(INFO) << "listen server ip = " << _ip << " port = " << _port;
  GatewayAccept::GetInstance().Init(_ip, _port);
  GatewayAccept::GetInstance().Listen();
  return 0;
}

static void timer_cb_loginterval(uv_timer_t* handle)
{
  GatewayProfiler::GetInstance().OnTimerLogInterval();
}

static void timer_cb_long(uv_timer_t* handle)
{
  GatewayManager::GetInstance().OnTimerLong();
}

static void timer_cb_middle(uv_timer_t* handle)
{
  GatewayAccept::GetInstance().UpdateStashTimeout();
  ClientAccept::GetInstance().UpdateStashTimeout();
  GatewayManager::GetInstance().OnTimerMiddle();
  GatewayProfiler::GetInstance().OnTimerMiddle();
}

static void timer_cb_short(uv_timer_t* handle)
{
  GatewayManager::GetInstance().OnTimerShort();
  UdpClientAcceptCenter::GetInstance().OnTimerShort();
}

int init_timer()
{
  short_timer_interval = 10;

  int r;
  r = uv_timer_init(uv_default_loop(), &timer_long);
  uv_timer_start(&timer_long, timer_cb_long, long_timer_interval, long_timer_interval);

  r = uv_timer_init(uv_default_loop(), &timer_short);
  uv_timer_start(&timer_short, timer_cb_short, short_timer_interval, short_timer_interval);

  r = uv_timer_init(uv_default_loop(), &timer_middle);
  uv_timer_start(&timer_middle, timer_cb_middle, middle_timer_interval, middle_timer_interval);

  r = uv_timer_init(uv_default_loop(), &timer_loginterval);
  uv_timer_start(&timer_loginterval, timer_cb_loginterval, log_timer_interval, log_timer_interval);
  return 0;
}

#ifdef WIN32
inline struct tm* localtime_r(const time_t* timep, struct tm* result) {
  localtime_s(result, timep);
  return result;
}
#endif  //! #ifdef WIN32

static bool init_logger()
{
  defaultLogger = new YLogger("yworldgateway");
  std::string logfilename;
  time_t start_time;
  time(&start_time);
  struct tm timeinfo;
  char now[128];
  localtime_r(&start_time, &timeinfo);
  std::string datetime = "%Y%m%d%H%M%S";
  strftime(now, sizeof(now), datetime.c_str(), &timeinfo);

  std::string filename = "yworldgateway_" + std::string(now) + ".log";
  defaultLogger->SetLogDir("../logs", filename);
  defaultLogger->SetLogLevel(Debug);
  defaultLogger->InitLogger();
  return true;
}

bool readConfig(const std::string &configFile, std::map<std::string, std::string> *out)
{
  std::ifstream config;
  config.open(configFile.c_str(), std::ios::in);
  if (!config.good())
  {
    LOG(ERROR) << "gateway.conf not exists or error. please check it.";
    return false;
  }
  while (!config.eof())
  {
    std::string line;
    getline(config, line);
    size_t pos = line.find_first_of("=");
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    (*out)[key] = value;
  }
  config.close();
  return true;
}

int main(int argc, const char **argv)
{
  init_logger();

#ifndef WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  bool ok = readConfig(cfg, &g_config);
  if (!ok)
  {
    LOG(ERROR) << "read gateway.conf fail";
    exit(-1);
  }

  LOG(ERROR) << "version: " << VERSION_STRING;

  int r;

  r = accept_client(g_config["ip"], atoi(g_config["port_client"].c_str()));
  UV_CHECK_RET_1(accept_client, r);

  r = accept_server(g_config["ip"], atoi(g_config["port_server"].c_str()));
  UV_CHECK_RET_1(accept_server, r);

  long_timer_interval   = atoi(g_config["long_timer_interval"].c_str());
  middle_timer_interval = atoi(g_config["short_timer_interval"].c_str());
  short_timer_interval  = atoi(g_config["middle_timer_interval"].c_str());
  log_timer_interval    = atoi(g_config["log_timer_interval"].c_str());

  init_timer();

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  UV_CHECK_RET_1(uv_run, r);

  return 0;
}

#endif  //! #ifndef _MAIN_H_

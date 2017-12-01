#include "dump_generator.h"
#include "spdlog/spdlog++.h"

#ifdef WIN32

google_breakpad::CrashGenerationServer* CrashServerStart(const std::wstring& pipe_name,
  google_breakpad::CrashGenerationServer::OnClientConnectedCallback connect_callback,
  void* connect_context,
  google_breakpad::CrashGenerationServer::OnClientDumpRequestCallback dump_callback,
  void* dump_context,
  google_breakpad::CrashGenerationServer::OnClientExitedCallback exit_callback,
  void* exit_context)
{
  std::wstring dump_path = L".\\dump";

  if (_wmkdir(dump_path.c_str()) && (errno != EEXIST)) {
    LOG(ERROR) << "Unable to create dump directory";
    return nullptr;
  }

  google_breakpad::CrashGenerationServer* crash_server = new google_breakpad::CrashGenerationServer(pipe_name,
    NULL,
    connect_callback,
    NULL,
    dump_callback,
    NULL,
    exit_callback,
    NULL,
    NULL,
    NULL,
    true,
    &dump_path);

  if (!crash_server->Start()) {
    LOG(ERROR) << "Unable to start server";
    delete crash_server;
    crash_server = NULL;
  }

  return crash_server;
}

void CrashServerStop(google_breakpad::CrashGenerationServer*& crash_server)
{
  delete crash_server;
  crash_server = NULL;
}

google_breakpad::ExceptionHandler* CrashClientStart(google_breakpad::ExceptionHandler::MinidumpCallback callback,
  void* callback_context,
  const wchar_t* pipe_name)
{
  std::wstring dump_path = L".\\dump";

  if (_wmkdir(dump_path.c_str()) && (errno != EEXIST)) {
    LOG(ERROR) << "Unable to create dump directory";
    return nullptr;
  }

  google_breakpad::ExceptionHandler* handle = new google_breakpad::ExceptionHandler(dump_path,
    NULL,
    callback,
    callback_context,
    google_breakpad::ExceptionHandler::HANDLER_ALL,
    MiniDumpNormal,
    pipe_name,
    NULL);

  return handle;
}

void CrashClientStop(google_breakpad::ExceptionHandler*& crash_client)
{
  delete crash_client;
  crash_client = NULL;
}

#else

#endif
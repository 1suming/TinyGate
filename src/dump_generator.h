#ifndef __DUMP_H__
#define __DUMP_H__

#include <string>

#ifdef WIN32

#include "client/windows/crash_generation/client_info.h"
#include "client/windows/crash_generation/crash_generation_server.h"
#include "client/windows/handler/exception_handler.h"
#include "client/windows/common/ipc_protocol.h"

google_breakpad::CrashGenerationServer* CrashServerStart(const std::wstring& pipe_name,
  google_breakpad::CrashGenerationServer::OnClientConnectedCallback connect_callback,
  void* connect_context,
  google_breakpad::CrashGenerationServer::OnClientDumpRequestCallback dump_callback,
  void* dump_context,
  google_breakpad::CrashGenerationServer::OnClientExitedCallback exit_callback,
  void* exit_context);

void CrashServerStop(google_breakpad::CrashGenerationServer*& crash_server);

google_breakpad::ExceptionHandler* CrashClientStart(google_breakpad::ExceptionHandler::MinidumpCallback callback,
  void* callback_context,
  const wchar_t* pipe_name);

void CrashClientStop(google_breakpad::ExceptionHandler*& crash_client);

#else

#include "client/linux/handler/exception_handler.h"
#include "third_party/lss/linux_syscall_support.h"

#endif

#endif  //__DUMP_H__

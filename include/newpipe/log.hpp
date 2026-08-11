#pragma once

#include <string>

namespace newpipe {

void init_log();
void shutdown_log();
void log_line(const std::string& message);
void logf(const char* format, ...);
// Tags the calling thread so a crash log line can name the faulting thread.
void set_thread_tag(const char* tag);
// Installs SIGSEGV/SIGABRT/SIGBUS/SIGILL/SIGFPE handlers that write signal,
// faulting address, thread id and thread tag to the log, then re-raise.
void install_fault_handler();

}  // namespace newpipe

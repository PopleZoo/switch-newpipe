#include "newpipe/log.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <unistd.h>

#ifdef __SWITCH__
#include <switch.h>
#include "switch/arm/thread_context.h"
#endif

namespace newpipe {
namespace {

std::mutex g_log_mutex;
FILE* g_log_file = nullptr;
// Duplicated for the crash handler: it may run while g_log_mutex is held, so
// it must use raw write() instead of logf().
int g_log_fd = -1;
thread_local const char* g_thread_tag = "unknown";

const char* log_path() {
#ifdef __SWITCH__
    return "sdmc:/switch/switch_newpipe.log";
#else
    return "switch_newpipe.log";
#endif
}

void write_all(int fd, const char* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        const ssize_t n = ::write(fd, data + written, len - written);
        if (n <= 0) {
            return;
        }
        written += static_cast<size_t>(n);
    }
}

void append_cstr(char* buf, size_t& pos, size_t cap, const char* s) {
    if (!s) {
        s = "null";
    }
    while (*s && pos + 1 < cap) {
        buf[pos++] = *s++;
    }
}

void append_uint(char* buf, size_t& pos, size_t cap, uintptr_t value, int base) {
    static const char kDigits[] = "0123456789abcdef";
    char tmp[2 * sizeof(uintptr_t) + 2];
    int i = 0;
    if (value == 0) {
        tmp[i++] = '0';
    } else {
        while (value > 0 && i < static_cast<int>(sizeof(tmp) - 2)) {
            tmp[i++] = kDigits[value % static_cast<uintptr_t>(base)];
            value /= static_cast<uintptr_t>(base);
        }
    }
    while (i > 0 && pos + 1 < cap) {
        buf[pos++] = tmp[--i];
    }
}

#if !defined(__SWITCH__)
void crash_handler(int sig) {
    const int fd = g_log_fd >= 0 ? g_log_fd : STDERR_FILENO;
    char buf[512];
    size_t pos = 0;
    append_cstr(buf, pos, sizeof(buf), "\n=== CRASH signal=");
    append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(sig), 10);
    append_cstr(buf, pos, sizeof(buf), " thread=0x");
    append_uint(buf, pos, sizeof(buf), reinterpret_cast<uintptr_t>(pthread_self()), 16);
    append_cstr(buf, pos, sizeof(buf), " tag=");
    append_cstr(buf, pos, sizeof(buf), g_thread_tag ? g_thread_tag : "unknown");
    append_cstr(buf, pos, sizeof(buf), " ===\n");
    write_all(fd, buf, pos);

    // Restore the default disposition and re-raise so the OS still produces
    // its normal fatal-error path (e.g. Atmosphère crash report).
    struct sigaction default_action;
    std::memset(&default_action, 0, sizeof(default_action));
    default_action.sa_handler = SIG_DFL;
    sigemptyset(&default_action.sa_mask);
    sigaction(sig, &default_action, nullptr);
    ::raise(sig);
    _exit(128 + sig);
}
#endif

}  // namespace

void init_log() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file) {
        return;
    }

    g_log_file = std::fopen(log_path(), "w");
    if (!g_log_file) {
        return;
    }

    g_log_fd = ::fileno(g_log_file);
    std::fprintf(g_log_file, "=== switch_newpipe log start ===\n");
    std::fflush(g_log_file);
}

void shutdown_log() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_log_file) {
        return;
    }

    std::fprintf(g_log_file, "=== switch_newpipe log end ===\n");
    std::fflush(g_log_file);
    std::fclose(g_log_file);
    g_log_file = nullptr;
    g_log_fd = -1;
}

void log_line(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_log_file) {
        return;
    }

    std::fprintf(g_log_file, "%s\n", message.c_str());
    std::fflush(g_log_file);
}

void logf(const char* format, ...) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_log_file) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vfprintf(g_log_file, format, args);
    va_end(args);
    std::fputc('\n', g_log_file);
    std::fflush(g_log_file);
}

void set_thread_tag(const char* tag) {
    g_thread_tag = tag ? tag : "unknown";
}

void install_fault_handler() {
#if !defined(__SWITCH__)
    // devkitA64 newlib declares sigaction but provides no implementation, so
    // on Switch hardware faults are only reachable through libnx's exception
    // hook below; this path still works on the host build.
    struct sigaction action;
    std::memset(&action, 0, sizeof(action));
    action.sa_handler = &crash_handler;
    sigemptyset(&action.sa_mask);
    const int signals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE};
    for (const int sig : signals) {
        sigaction(sig, &action, nullptr);
    }
#endif
    log_line("main: fault handler installed");
}

#ifdef __SWITCH__
// libnx calls this weak hook when the kernel reports a hardware exception on
// any thread. newlib sigaction cannot deliver hardware faults on Switch, so
// this is the only path that sees the faulting PC/SP/thread id.
//
// Layout notes (from libnx exception.o on this toolchain): before resuming
// the faulting thread at this hook, libnx's exception entry builds a break
// image in the __nx_exceptiondump global. Reading the hook's ctx argument is
// NOT reliable (it is kernel scratch memory that libnx rewrites); the dump
// below mirrors exactly the fields libnx copied:
//   +0    u32   error_desc
//   +16   u64 x [10]   kernel-saved registers (ctx[0..63], ctx[72..79])
//   +88   u64 x [21]   x9..x28 then fp (x29)
//   +264  u64   sp     faulting stack pointer
//   +272  u64   pc     faulting program counter
//   +280  u64   0      (libnx nils the CPSR slot)
//   +800  u32 xs[4]    fpsr/fpcr/etc (ctx[96..111])
extern "C" {
extern unsigned char __nx_exceptiondump[];
}

static uint64_t dump_u64(size_t offset) {
    uint64_t value = 0;
    std::memcpy(&value, __nx_exceptiondump + offset, sizeof(value));
    return value;
}

extern "C" void __libnx_exception_handler(ThreadContext* /*ctx*/) {
    const int fd = g_log_fd >= 0 ? g_log_fd : STDERR_FILENO;
    static int g_handler_entries = 0;
    const int entry = ++g_handler_entries;
    char buf[2048];
    size_t pos = 0;

    append_cstr(buf, pos, sizeof(buf), "\n=== CRASH entry=");
    append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(entry), 10);
    append_cstr(buf, pos, sizeof(buf), " err=0x");
    append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(0) & 0xffffffffU), 16);
    append_cstr(buf, pos, sizeof(buf), " pc=0x");
    append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(272)), 16);
    append_cstr(buf, pos, sizeof(buf), " sp=0x");
    append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(264)), 16);
    append_cstr(buf, pos, sizeof(buf), " tag=");
    append_cstr(buf, pos, sizeof(buf), g_thread_tag ? g_thread_tag : "unknown");
    append_cstr(buf, pos, sizeof(buf), " ===\n");
    write_all(fd, buf, pos);
    pos = 0;

    append_cstr(buf, pos, sizeof(buf), "=== module marker log_line=");
    append_uint(buf, pos, sizeof(buf), reinterpret_cast<uintptr_t>(&log_line), 16);
    append_cstr(buf, pos, sizeof(buf), "\n");

    append_cstr(buf, pos, sizeof(buf), "kctx r1..r10:");
    for (int i = 0; i < 8; ++i) {
        append_cstr(buf, pos, sizeof(buf), " 0x");
        append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(16 + static_cast<size_t>(i) * 8)), 16);
    }
    append_cstr(buf, pos, sizeof(buf), " 0x");
    append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(80)), 16);
    append_cstr(buf, pos, sizeof(buf), " 0x");
    append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(256)), 16);
    append_cstr(buf, pos, sizeof(buf), "\ngprs x9..fp:");
    for (int i = 0; i < 21; ++i) {
        append_cstr(buf, pos, sizeof(buf), " 0x");
        append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(88 + static_cast<size_t>(i) * 8)), 16);
    }
    append_cstr(buf, pos, sizeof(buf), "\nfpsr/fpcr:");
    for (int i = 0; i < 4; ++i) {
        append_cstr(buf, pos, sizeof(buf), " 0x");
        append_uint(buf, pos, sizeof(buf), static_cast<uintptr_t>(dump_u64(800 + static_cast<size_t>(i) * 4) & 0xffffffffU), 16);
    }
    append_cstr(buf, pos, sizeof(buf), "\n");

    write_all(fd, buf, pos);
    if (g_log_fd >= 0) {
        ::fsync(g_log_fd);
    }
    svcExitProcess();
}
#endif

}  // namespace newpipe

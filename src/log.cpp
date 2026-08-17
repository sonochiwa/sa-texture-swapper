#include "log.h"

#include <windows.h>

#include <io.h>
#include <share.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace logging {
namespace {

FILE*      g_file = nullptr;
std::mutex g_mutex;

} // namespace

void Open(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file)
        return;
    // _SH_DENYWR lets the log be read while the game holds it open.
    g_file = _wfsopen(path.c_str(), L"w", _SH_DENYWR);
}

void Close() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
}

void Write(const char* tag, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(g_file, "[%02u:%02u:%02u.%03u] [%s] ", st.wHour, st.wMinute, st.wSecond,
            st.wMilliseconds, tag);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_file, fmt, args);
    va_end(args);

    fputc('\n', g_file);
    fflush(g_file);
    // Push the file size to disk too, so a reader outside the game sees the log grow.
    _commit(_fileno(g_file));
}

} // namespace logging

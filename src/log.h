#pragma once

#include <string>

namespace logging {

// Opens the log file. Until this is called nothing is written anywhere, which is
// what keeps logging off unless the INI asks for it.
void Open(const std::wstring& path);
void Close();

void Write(const char* tag, const char* fmt, ...);

} // namespace logging

#define LOG_ERROR(...) ::logging::Write("ERROR", __VA_ARGS__)
#define LOG_INFO(...)  ::logging::Write("INFO ", __VA_ARGS__)

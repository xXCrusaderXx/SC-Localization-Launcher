#pragma once

// The few calls that differ between Linux and Windows. Linux keeps the POSIX and /proc calls it always used.

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#else
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace Platform
{
inline std::tm localTime (std::time_t time)
{
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    return local;
}

// Seconds since the epoch for a broken-down UTC time
inline std::time_t fromUtc (std::tm& utc)
{
#ifdef _WIN32
    return _mkgmtime(&utc);
#else
    return timegm(&utc);
#endif
}

// Whether the current user may write into this file or folder
inline bool isWritable (const std::filesystem::path& path)
{
#ifdef _WIN32
    return _waccess(path.c_str(), 2) == 0;
#else
    return access(path.c_str(), W_OK) == 0;
#endif
}

// Environment variable, empty if it is not set
inline std::filesystem::path environmentPath (const char* name)
{
#ifdef _WIN32
    // The wide variant keeps non-ASCII user names intact
    std::wstring wide(name, name + std::char_traits<char>::length(name));
    if(const wchar_t* value = _wgetenv(wide.c_str()); value && *value) return std::filesystem::path(value);
#else
    if(const char* value = std::getenv(name); value && *value) return std::filesystem::path(value);
#endif
    return {};
}

// Folder of the running program; nullopt if it cannot be found
inline std::optional<std::filesystem::path> executableFolder ()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for(;;)
    {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if(length == 0) return std::nullopt;
        if(length < buffer.size()) return std::filesystem::path(buffer.substr(0, length)).parent_path();
        buffer.resize(buffer.size() * 2);
    }
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    // readlink() does not null-terminate
    if(count != -1) return std::filesystem::path(std::string(result, count)).parent_path();
    return std::nullopt;
#endif
}

// A value that changes when the process id is given to another process; 0 if no such process runs
inline uint64_t processStartTime (int64_t pid)
{
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if(!process) return 0;
    FILETIME created{}, exited{}, kernel{}, user{};
    DWORD code = 0;
    bool running = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    bool known = GetProcessTimes(process, &created, &exited, &kernel, &user);
    CloseHandle(process);
    if(!running || !known) return 0;
    return (static_cast<uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
#else
    std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    if(!std::getline(stat, line)) return 0;
    // "pid (name) state ppid …"; the name may contain spaces and parentheses, starttime is field 22
    auto end = line.rfind(')');
    if(end == std::string::npos) return 0;
    std::istringstream fields(line.substr(end + 2));
    std::string field;
    for(int i = 3; i <= 22 && fields >> field; ++i)
        if(i == 22) return std::stoull(field);
    return 0;
#endif
}

// Whether the process still runs; a zombie that its parent has not reaped yet does not count
inline bool processRuns (int64_t pid)
{
#ifdef _WIN32
    return processStartTime(pid) != 0;
#else
    std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    if(!std::getline(stat, line)) return false;
    // "pid (name) state …"; the name may contain spaces and parentheses
    auto end = line.rfind(')');
    return end != std::string::npos && end + 2 < line.size() && line[end + 2] != 'Z' && line[end + 2] != 'X';
#endif
}

// Asks the process to end. On Linux the process group goes with it when the process leads one, so the children of
// a script end too. Windows has no such request for programs without a window, so the process is ended there.
inline void endProcess (int64_t pid)
{
#ifdef _WIN32
    if(HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid)))
    {
        TerminateProcess(process, 1);
        CloseHandle(process);
    }
#else
    if(::kill(static_cast<pid_t>(-pid), SIGTERM) != 0) ::kill(static_cast<pid_t>(pid), SIGTERM);
#endif
}
}  // namespace Platform

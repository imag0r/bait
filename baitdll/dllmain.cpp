#include <Windows.h>
#include <tlhelp32.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>

#include "scoped_handle.h"

typedef std::map<DWORD, PROCESSENTRY32W> process_snapshot;

inline void throw_win32_error(const char* msg, DWORD error = ::GetLastError())
{
    throw std::system_error(std::error_code(error, std::system_category()), msg);
}

inline void throw_win32_error_if(bool condition, const char* msg, DWORD error = ::GetLastError())
{
    if (condition)
    {
        throw_win32_error(msg, error);
    }
}

process_snapshot snapshot_processes()
{
    scoped_handle handle = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    throw_win32_error_if(!handle.valid(), "CreateToolhelp32Snapshot");

    process_snapshot snapshot;
    PROCESSENTRY32W entry = { 0 };
    entry.dwSize = sizeof(entry);
    throw_win32_error_if(!::Process32FirstW(handle, &entry), "Process32FirstW");

    for (;;)
    {
        snapshot.emplace(std::make_pair(entry.th32ProcessID, entry));
        if (!::Process32NextW(handle, &entry))
        {
            const auto error = ::GetLastError();
            if (error == ERROR_NO_MORE_FILES)
            {
                break;
            }
            throw_win32_error_if(!handle.valid(), "Process32NextW", error);
        }
    }
    return snapshot;
}

std::wstring user_name()
{
    DWORD size = 0;
    ::GetUserNameW(nullptr, &size);
    std::wstring name(size, 0);
    throw_win32_error_if(!::GetUserNameW(&name[0], &size), "GetUserNameW");
    name.pop_back();
    return name;
}

std::wstring integrity_level()
{
    HANDLE hToken = nullptr;

    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        return L"<unknown>";
    }

    scoped_handle token = hToken;

    DWORD size = 0;

    ::GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
    std::vector<BYTE> buffer(size, 0);

    auto til = reinterpret_cast<PTOKEN_MANDATORY_LABEL>(buffer.data());

    if (!::GetTokenInformation(token, TokenIntegrityLevel, til, size, &size))
    {
        return L"<unknown>";
    }

    const auto level = *::GetSidSubAuthority(til->Label.Sid, *GetSidSubAuthorityCount(til->Label.Sid) - 1);
    
    auto display_level = std::to_wstring(level) + L" ";

    if (level == SECURITY_MANDATORY_LOW_RID)
    {
        display_level += L"(low)";
    }
    else if ((level >= SECURITY_MANDATORY_MEDIUM_RID) && (level < SECURITY_MANDATORY_HIGH_RID))
    {
        display_level += L"(medium)";
    }
    else if ((level >= SECURITY_MANDATORY_HIGH_RID) && (level < SECURITY_MANDATORY_SYSTEM_RID))
    {
        display_level += L"(high)";
    }
    else if (level >= SECURITY_MANDATORY_SYSTEM_RID)
    {
        display_level += L"(system)";
    }

    return display_level;
}

extern "C" IMAGE_DOS_HEADER __ImageBase;

std::wstring module_path(HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase))
{
    std::wstring path(MAX_PATH, 0);
    DWORD size = ::GetModuleFileNameW(module, &path[0], path.size());
    path.resize(size);
    return path;
}

std::wstring executable_path()
{
    return module_path(nullptr);
}

std::wstring timestamp()
{
    auto itt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::wostringstream ss;
    tm t;
    gmtime_s(&t, &itt);
    ss << std::put_time(&t, L"%FT%TZ");
    return ss.str();
}

std::wstring default_output_path()
{
    std::wstring path(MAX_PATH, 0);
    auto size = ::GetWindowsDirectoryW(&path[0], static_cast<UINT>(path.size()));
    throw_win32_error_if(!size, "GetWindowsDirectoryW");
    path.resize(size);
    path += L"\\Temp\\bait.txt";
    return path;
}

std::wstring output_path()
{
    const auto ini_path = module_path() + L".ini";

    std::wstring value(4096, 0);
    const auto size = ::GetPrivateProfileStringW(L"bait", L"log", default_output_path().c_str(), &value[0], static_cast<DWORD>(value.size()), ini_path.c_str());
    value.resize(size);
    return value;
}

std::string utf16_to_utf8(const std::wstring& str)
{
    const DWORD codepage = CP_UTF8;

    int size = ::WideCharToMultiByte(codepage, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    throw_win32_error_if(!size, "WideCharToMultiByte");

    std::string utf8(size, 0);
    size = ::WideCharToMultiByte(codepage, 0, str.c_str(), -1, &utf8[0], static_cast<int>(utf8.size()), nullptr, nullptr);
    throw_win32_error_if(!size, "WideCharToMultiByte");

    utf8.pop_back();
    return utf8;
}

void append_text_to_file(const std::wstring& path, const std::wstring& text)
{
    scoped_handle mutex = ::CreateMutexW(nullptr, FALSE, L"Global\\BAIT_C5E8A8E9AE9D49BA8CB306C695F11ABD");
    throw_win32_error_if(!mutex.valid(), "CreateMutexW");
    throw_win32_error_if(WAIT_OBJECT_0 != ::WaitForSingleObject(mutex, INFINITE), "WaitForSingleObject");

    scoped_file_handle file = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    throw_win32_error_if(!file.valid(), "CreateFileW");

    throw_win32_error_if(INVALID_SET_FILE_POINTER == ::SetFilePointer(file, 0, nullptr, FILE_END), "SetFilePointer");

    const auto text_utf8 = utf16_to_utf8(text);
    DWORD written = 0;
    throw_win32_error_if(!::WriteFile(file, text_utf8.data(), static_cast<DWORD>(text_utf8.size()), &written, nullptr), "WriteFile");
}

bool running_in_console()
{
    return ::GetConsoleWindow() != nullptr;
}

void log_execution()
{
    try
    {
        const auto snapshot = snapshot_processes();
        const auto pid = ::GetCurrentProcessId();

        std::wostringstream ss;
        ss << timestamp() << "\r\n"
            L"\tUser:       " << user_name() << "\r\n"
            L"\tPID:        " << pid << L" " << "\r\n"
            L"\tIntegrity:  " << integrity_level() << L" " << "\r\n"
            L"\tExe:        " << executable_path() << "\r\n"
            L"\tModule:     " << module_path() << "\r\n"
            L"\tCmdline:    " << ::GetCommandLineW() << L"\r\n"
            L"\tCall:       ";
        auto it = snapshot.find(pid);
        for (;;)
        {
            ss << it->second.szExeFile << L" (" << it->second.th32ProcessID << L")";
            it = snapshot.find(it->second.th32ParentProcessID);
            if (it == snapshot.end())
            {
                ss << L"\r\n";
                break;
            }
            ss << L" <- ";
        }

        const auto output = ss.str();
        if (running_in_console())
        {
            std::wcout << output;
        }

        const auto output_file = output_path();
        if (!output_file.empty())
        {
            append_text_to_file(output_file, ss.str());
        }
    }
    catch (const std::exception& exc)
    {
        if (running_in_console())
        {
            std::cerr << "Exception: " << exc.what() << std::endl;
        }
    }
}

#ifdef _USRDLL

DWORD WINAPI ThreadProc(_In_ LPVOID lpParameter)
{
    log_execution();
    return 0;
}

HANDLE g_thread = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_thread = ::CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        if (g_thread)
        {
            ::WaitForSingleObject(g_thread, INFINITE);
            ::CloseHandle(g_thread);
            g_thread = nullptr;
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

#else
int wmain(int /*argc*/, wchar_t* /*argv*/[])
{
    log_execution();
    return 0;
}
#endif

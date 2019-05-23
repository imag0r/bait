#include <Windows.h>
#include <tlhelp32.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>

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

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		const auto snapshot = snapshot_processes();
		const auto pid = ::GetCurrentProcessId();

		std::wostringstream ss;
		ss << timestamp() << L" " << user_name() << L" " << pid << L" " << ::GetCommandLineW() << L"\n\t";
		auto it = snapshot.find(pid);
		for (;;)
		{
			ss << it->second.szExeFile << L" (" << it->second.th32ProcessID << L")";
			if (it->second.th32ParentProcessID == 0)
			{
				break;
			}
			it = snapshot.find(it->second.th32ParentProcessID);
			if (it == snapshot.end())
			{
				ss << L"\n";
				break;
			}
			ss << L" <- ";
		}
	
		const auto output = ss.str();
		std::wcout << output;
		append_text_to_file(default_output_path(), ss.str());

		return 0;
	}
	catch (const std::exception& exc)
	{
		std::cerr << "Exception: " << exc.what() << std::endl;
		return 1;
	}
}

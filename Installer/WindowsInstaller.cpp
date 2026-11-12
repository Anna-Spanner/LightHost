#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#include <vector>

namespace
{
constexpr int appResource = 101;

std::wstring systemError (DWORD error)
{
    wchar_t* buffer = nullptr;
    FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                    | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
                    reinterpret_cast<wchar_t*> (&buffer), 0, nullptr);
    std::wstring result = buffer != nullptr ? buffer : L"Unknown Windows error.";
    if (buffer != nullptr)
        LocalFree (buffer);
    return result;
}

bool ensureDirectory (const std::wstring& path, std::wstring& error)
{
    const auto status = SHCreateDirectoryExW (nullptr, path.c_str(), nullptr);
    if (status == ERROR_SUCCESS || status == ERROR_ALREADY_EXISTS || status == ERROR_FILE_EXISTS)
        return true;
    error = L"Could not create " + path + L": " + systemError (status);
    return false;
}

bool install (const std::wstring& directory, bool createShortcut, std::wstring& error)
{
    if (! ensureDirectory (directory, error))
        return false;
    const auto resource = FindResourceW (nullptr, MAKEINTRESOURCEW (appResource), RT_RCDATA);
    const auto loaded = resource != nullptr ? LoadResource (nullptr, resource) : nullptr;
    const auto data = loaded != nullptr ? LockResource (loaded) : nullptr;
    const DWORD size = resource != nullptr ? SizeofResource (nullptr, resource) : 0;
    if (data == nullptr || size == 0)
    {
        error = L"The installer does not contain the application.";
        return false;
    }

    wchar_t temporary[MAX_PATH] {};
    if (GetTempFileNameW (directory.c_str(), L"lh", 0, temporary) == 0)
    {
        error = L"Could not create a temporary installation file: " + systemError (GetLastError());
        return false;
    }
    HANDLE file = CreateFileW (temporary, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        error = L"Could not open the temporary installation file: " + systemError (GetLastError());
        DeleteFileW (temporary);
        return false;
    }
    DWORD written = 0;
    const bool success = WriteFile (file, data, size, &written, nullptr) != FALSE
                      && written == size && FlushFileBuffers (file) != FALSE;
    const DWORD writeError = success ? ERROR_SUCCESS : GetLastError();
    CloseHandle (file);
    if (! success)
    {
        DeleteFileW (temporary);
        error = L"Could not write the application: " + systemError (writeError);
        return false;
    }
    const std::wstring executable = directory + L"\\Light Host.exe";
    if (! MoveFileExW (temporary, executable.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const auto moveError = GetLastError();
        DeleteFileW (temporary);
        error = L"Could not install or upgrade Light Host. Close all running Light Host instances and try again.\n\n"
                + systemError (moveError);
        return false;
    }
    if (! createShortcut)
        return true;

    PWSTR programs = nullptr;
    HRESULT hr = SHGetKnownFolderPath (FOLDERID_Programs, KF_FLAG_CREATE, nullptr, &programs);
    if (FAILED (hr))
    {
        error = L"Light Host was installed, but the Start menu folder could not be found.";
        return false;
    }
    const std::wstring linkPath = std::wstring (programs) + L"\\Light Host.lnk";
    CoTaskMemFree (programs);
    IShellLinkW* link = nullptr;
    hr = CoCreateInstance (CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&link));
    if (SUCCEEDED (hr))
    {
        hr = link->SetPath (executable.c_str());
        if (SUCCEEDED (hr)) hr = link->SetWorkingDirectory (directory.c_str());
        IPersistFile* persist = nullptr;
        if (SUCCEEDED (hr)) hr = link->QueryInterface (IID_PPV_ARGS (&persist));
        if (SUCCEEDED (hr))
        {
            hr = persist->Save (linkPath.c_str(), TRUE);
            persist->Release();
        }
        link->Release();
    }
    if (FAILED (hr))
    {
        error = L"Light Host was installed, but its Start menu shortcut could not be created.";
        return false;
    }
    return true;
}
}

int WINAPI wWinMain (HINSTANCE, HINSTANCE, PWSTR, int)
{
    int count = 0;
    auto args = CommandLineToArgvW (GetCommandLineW(), &count);
    if (args == nullptr)
        return 1;
    std::wstring directory;
    bool shortcut = true;
    bool valid = true;
    for (int i = 1; i < count; ++i)
    {
        const std::wstring arg = args[i];
        if (arg == L"--directory" && i + 1 < count)
            directory = args[++i];
        else if (arg == L"--no-shortcut")
            shortcut = false;
        else
            valid = false;
    }
    LocalFree (args);
    if (! valid)
    {
        MessageBoxW (nullptr, L"Usage: Light Host Setup.exe [--directory path] [--no-shortcut]",
                     L"Light Host Setup", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (directory.empty())
    {
        PWSTR local = nullptr;
        if (FAILED (SHGetKnownFolderPath (FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local)))
        {
            MessageBoxW (nullptr, L"Could not find the local application data folder.",
                         L"Light Host Setup", MB_OK | MB_ICONERROR);
            return 1;
        }
        directory = std::wstring (local) + L"\\Programs\\Light Host";
        CoTaskMemFree (local);
    }
    if (MessageBoxW (nullptr, (L"Install or upgrade Light Host in:\n\n" + directory
                               + L"\n\nExisting plugin settings will be kept. Close Light Host before continuing.").c_str(),
                     L"Light Host Setup", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
        return 2;
    const auto comResult = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED (comResult))
    {
        MessageBoxW (nullptr, L"Could not initialize Windows installer services.",
                     L"Light Host Setup", MB_OK | MB_ICONERROR);
        return 1;
    }
    std::wstring error;
    const bool installed = install (directory, shortcut, error);
    CoUninitialize();
    if (! installed)
    {
        MessageBoxW (nullptr, error.c_str(), L"Light Host Setup", MB_OK | MB_ICONERROR);
        return 1;
    }
    MessageBoxW (nullptr, L"Light Host was installed successfully.", L"Light Host Setup",
                 MB_OK | MB_ICONINFORMATION);
    return 0;
}

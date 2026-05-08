#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

const wchar_t* SINGLE_INSTANCE_MUTEX = L"Global\\Lab1SingleInstanceMutex";
const wchar_t* SEMAPHORE_NAME = L"Global\\Lab1Semaphore";

void PrintError(const string& text)
{
    cout << text << " GetLastError = " << GetLastError() << endl;
}

void ChildProcess(int number, HANDLE inheritedMutex)
{
    HANDLE hSemaphore = OpenSemaphoreW(
        SEMAPHORE_ALL_ACCESS,
        FALSE,
        SEMAPHORE_NAME
    );

    if (hSemaphore == NULL)
    {
        PrintError("Child: OpenSemaphore failed.");
        return;
    }

    WaitForSingleObject(hSemaphore, INFINITE);

    WaitForSingleObject(inheritedMutex, INFINITE);

    cout << "Child process number: " << number
        << ", PID = " << GetCurrentProcessId() << endl;

    ReleaseMutex(inheritedMutex);

    Sleep(3000);

    ReleaseSemaphore(hSemaphore, 1, NULL);

    CloseHandle(hSemaphore);
}

void ParentProcess()
{
    HANDLE hSingleInstanceMutex = CreateMutexW(
        NULL,
        TRUE,
        SINGLE_INSTANCE_MUTEX
    );

    if (hSingleInstanceMutex == NULL)
    {
        PrintError("CreateMutex for single instance failed.");
        return;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        cout << "Program is already running. This copy will exit." << endl;
        CloseHandle(hSingleInstanceMutex);
        system("pause");
        return;
    }

    cout << "Main process started. PID = "
        << GetCurrentProcessId() << endl;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hUnnamedMutex = CreateMutexW(
        &sa,
        FALSE,
        NULL
    );

    if (hUnnamedMutex == NULL)
    {
        PrintError("Create unnamed mutex failed.");
        ReleaseMutex(hSingleInstanceMutex);
        CloseHandle(hSingleInstanceMutex);
        return;
    }

    HANDLE hSemaphore = CreateSemaphoreW(
        NULL,
        3,
        3,
        SEMAPHORE_NAME
    );

    if (hSemaphore == NULL)
    {
        PrintError("CreateSemaphore failed.");
        CloseHandle(hUnnamedMutex);
        ReleaseMutex(hSingleInstanceMutex);
        CloseHandle(hSingleInstanceMutex);
        return;
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    const int COUNT = 10;

    PROCESS_INFORMATION pi[COUNT] = {};
    STARTUPINFOW si[COUNT] = {};
    HANDLE processHandles[COUNT] = {};

    for (int i = 0; i < COUNT; i++)
    {
        si[i].cb = sizeof(STARTUPINFOW);

        wstring commandLine = L"\"";
        commandLine += exePath;
        commandLine += L"\" child ";
        commandLine += to_wstring(i + 1);
        commandLine += L" ";
        commandLine += to_wstring((unsigned long long)hUnnamedMutex);

        wchar_t buffer[512];
        wcscpy_s(buffer, 512, commandLine.c_str());

        BOOL result = CreateProcessW(
            NULL,
            buffer,
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            NULL,
            &si[i],
            &pi[i]
        );

        if (result)
        {
            cout << "Created child process #" << i + 1
                << ", PID = " << pi[i].dwProcessId << endl;

            processHandles[i] = pi[i].hProcess;
        }
        else
        {
            PrintError("CreateProcess failed.");
        }
    }

    HANDLE hTimer = CreateWaitableTimerW(
        NULL,
        TRUE,
        NULL
    );

    if (hTimer == NULL)
    {
        PrintError("CreateWaitableTimer failed.");
    }
    else
    {
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -5LL * 10000000LL;

        if (!SetWaitableTimer(
            hTimer,
            &dueTime,
            0,
            NULL,
            NULL,
            FALSE
        ))
        {
            PrintError("SetWaitableTimer failed.");
        }

        cout << endl;
        cout << "Main process waits 5 seconds using timer..." << endl;

        WaitForSingleObject(hTimer, INFINITE);

        cout << endl;
        cout << "Timer finished. Checking child processes..." << endl;

        for (int i = 0; i < COUNT; i++)
        {
            DWORD result = WaitForSingleObject(
                processHandles[i],
                0
            );

            if (result == WAIT_OBJECT_0)
            {
                cout << "Child process #" << i + 1
                    << " has finished." << endl;
            }
            else if (result == WAIT_TIMEOUT)
            {
                cout << "Child process #" << i + 1
                    << " is still running." << endl;
            }
            else
            {
                PrintError("WaitForSingleObject failed.");
            }
        }

        CloseHandle(hTimer);
    }

    WaitForMultipleObjects(
        COUNT,
        processHandles,
        TRUE,
        INFINITE
    );

    cout << endl;
    cout << "All child processes are finished now." << endl;

    for (int i = 0; i < COUNT; i++)
    {
        if (pi[i].hProcess != NULL)
            CloseHandle(pi[i].hProcess);

        if (pi[i].hThread != NULL)
            CloseHandle(pi[i].hThread);
    }

    CloseHandle(hSemaphore);
    CloseHandle(hUnnamedMutex);

    cout << "Main process finished." << endl;

    system("pause");

    ReleaseMutex(hSingleInstanceMutex);
    CloseHandle(hSingleInstanceMutex);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc >= 4 && wcscmp(argv[1], L"child") == 0)
    {
        int number = _wtoi(argv[2]);

        HANDLE inheritedMutex = (HANDLE)_wcstoui64(argv[3], NULL, 10);

        ChildProcess(number, inheritedMutex);

        return 0;
    }
    else
    {
        ParentProcess();

        return 0;
    }
}
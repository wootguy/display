#include <windows.h> 
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>

#include "mmlib.h"

#define BUFSIZE 4096

using namespace std;

struct ProcessData {
    uint32_t subpid;
    HANDLE hStd_OUT_Rd = NULL;
    HANDLE hStd_OUT_Wr = NULL;
    HANDLE hProcess = NULL;
};

uint32_t g_subpid = 1;
vector<ProcessData> g_process_data;

// Create a child process that uses the previously created pipes for STDIN and STDOUT.
uint32_t createChildProcess(const char* command)
{
    SECURITY_ATTRIBUTES saAttr;
    ProcessData pdata;

    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT. 
    if (!CreatePipe(&pdata.hStd_OUT_Rd, &pdata.hStd_OUT_Wr, &saAttr, 0)) {
        println("ERROR: StdoutRd CreatePipe");
        return 0;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(pdata.hStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        println("ERROR: Stdout SetHandleInformation");
        return 0;
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;

    // Set up members of the PROCESS_INFORMATION structure. 

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDIN and STDOUT handles for redirection.

    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = pdata.hStd_OUT_Wr;
    siStartInfo.hStdOutput = pdata.hStd_OUT_Wr;
    //siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Create the child process. 

    bSuccess = CreateProcess(NULL,
        (TCHAR*)command,     // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        CREATE_NO_WINDOW,             // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION 

     // If an error occurs, exit the application. 
    if (!bSuccess) {
        return 0;
    }
    else
    {
        // Close handles to the child process and its primary thread.
        // Some applications might keep these handles to monitor the status
        // of the child process, for example. 

        //CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
        pdata.hProcess = piProcInfo.hProcess;

        // Close handles to the stdin and stdout pipes no longer needed by the child process.
        // If they are not explicitly closed, there is no way to recognize that the child process has ended.

        CloseHandle(pdata.hStd_OUT_Wr);
        //CloseHandle(g_hChildStd_IN_Rd);
    }

    pdata.subpid = g_subpid++;
    g_process_data.push_back(pdata);

    return pdata.subpid;
}

ProcessData* findSubprocess(int subpid) {
    for (int i = 0; i < g_process_data.size(); i++) {
        if (g_process_data[i].subpid == subpid) {
            return &g_process_data[i];
        }
    }

    return NULL;
}

bool isProcessAlive(int subpid) {
    ProcessData* pdata = findSubprocess(subpid);
    if (!pdata) {
        println("Invalid subprocess id %d", subpid);
        return false;
    }

    DWORD exitCode;
    GetExitCodeProcess(pdata->hProcess, &exitCode);

    return exitCode == STILL_ACTIVE;
}

int peekChildProcessStdout(int subpid, char* outputBuffer, int bytesWanted) {
    ProcessData* pdata = findSubprocess(subpid);
    if (!pdata) {
        println("Invalid subprocess id %d", subpid);
        return false;
    }

    DWORD dwRead;
    DWORD available;
    DWORD bytesLeft;
    PeekNamedPipe(pdata->hStd_OUT_Rd, outputBuffer, bytesWanted, &dwRead, &available, &bytesLeft);

    return available;
}

// Read output from the child process's pipe for STDOUT
// and write to the parent process's pipe for STDOUT. 
// Stop when there is no more data.
bool readChildProcessStdout(int subpid, char* outputBuffer, int bytesWanted, int& bytesRead, bool block)
{
    ProcessData* pdata = findSubprocess(subpid);
    if (!pdata) {
        println("Invalid subprocess id %d", subpid);
        return false;
    }

    bytesRead = 0;
    while (bytesWanted) {
        if (!block && peekChildProcessStdout(subpid, outputBuffer + bytesRead, bytesWanted) == 0) {
            return false;
        }

        DWORD dwRead;
        if (!ReadFile(pdata->hStd_OUT_Rd, outputBuffer + bytesRead, bytesWanted, &dwRead, NULL)) {
            println("Read file error %d", (int)GetLastError());
            return false;
        }
        
        bytesRead += dwRead;
        bytesWanted -= dwRead;
    }
    

    return true;
}

bool killChildProcess(int subpid) {
    ProcessData* pdata = findSubprocess(subpid);
    if (!pdata) {
        println("Invalid subprocess id %d", subpid);
        return false;
    }

    TerminateProcess(pdata->hProcess, 0);

    CloseHandle(pdata->hProcess);
    CloseHandle(pdata->hStd_OUT_Rd);
    //CloseHandle(pdata->hStd_OUT_Wr);
    pdata->hProcess = NULL;

    return true;
}

void killAllChildren() {
    for (int i = 0; i < g_process_data.size(); i++) {
        killChildProcess(g_process_data[i].subpid);
    }

    /*
    vector<ProcessData> newPdata;
    for (int i = 0; i < g_process_data.size(); i++) {
        if (g_process_data[i].hProcess) {
            newPdata.push_back(g_process_data[i]);
        }
    }

    g_process_data = newPdata;
    */

    g_process_data.clear();
}
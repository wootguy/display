#include "mmlib.h"
#include "subprocess.h"

using namespace std;

struct ProcessData {
    uint32_t subpid;
};

uint32_t g_subpid = 1;
vector<ProcessData> g_process_data;

// Create a child process that uses the previously created pipes for STDIN and STDOUT.
uint32_t createChildProcess(const char* command)
{
    ProcessData pdata;

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

    // todo

    return false;
}

int peekChildProcessStdout(int subpid, char* outputBuffer, int bytesWanted) {
    ProcessData* pdata = findSubprocess(subpid);
    if (!pdata) {
        println("Invalid subprocess id %d", subpid);
        return false;
    }

    // todo
   
    return 0;
}

bool readChildProcessStdout(int subpid, char* outputBuffer, int bytesWanted, int& bytesRead)
{
    ProcessData* pdata = findSubprocess(subpid);
    if (!pdata) {
        println("Invalid subprocess id %d", subpid);
        return false;
    }

    // todo

    return false;
}

bool killChildProcess(int subpid) {
    ProcessData* pdata = findSubprocess(subpid);
    if (!pdata) {
        println("Invalid subprocess id %d", subpid);
        return false;
    }

    // todo

    return true;
}

void killAllChildren() {
    for (int i = 0; i < g_process_data.size(); i++) {
        killChildProcess(g_process_data[i].subpid);
    }

    g_process_data.clear();
}
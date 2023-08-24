#include <vector>

// returns an ID that can be used with the other subprocess commands
// returns 0 on failure
uint32_t createChildProcess(const char* command);

// read data from process output
// returns false on failure
bool readChildProcessStdout(int subpid, char* outputBuffer, int bytesWanted, int& bytesRead, bool block=false);

bool isProcessAlive(int subpid);

bool killChildProcess(int subpid);

void killAllChildren();
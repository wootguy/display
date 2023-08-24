#include "mmlib.h"
#include "subprocess.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>

using namespace std;

struct ProcessData {
	uint32_t subpid;
	int hStd_OUT_Rd;
	int hStd_OUT_Wr;
	int hProcess;
};

uint32_t g_subpid = 1;
vector<ProcessData> g_process_data;

// Create a child process that uses the previously created pipes for STDIN and STDOUT.
uint32_t createChildProcess(const char* command)
{
	ProcessData pdata;

	int pipe_fd[2];
	if (pipe(pipe_fd) == -1) {
		perror("pipe");
		return 0;
	}
	pdata.hStd_OUT_Rd = pipe_fd[0];
	pdata.hStd_OUT_Wr = pipe_fd[1];

	pid_t childPid = fork();
	if (childPid == -1) {
		perror("fork");
		return 0;
	}

	if (childPid == 0) { // Child process
		close(pdata.hStd_OUT_Rd);
		dup2(pdata.hStd_OUT_Wr, STDOUT_FILENO);
		close(pdata.hStd_OUT_Wr);

		// Replace "sh" with your desired shell (e.g., "/bin/sh" or "/bin/bash")
		execl("/bin/sh", "sh", "-c", command, nullptr);

		// If exec fails
		perror("exec");
		_exit(127);
	} else { // Parent process
		close(pdata.hStd_OUT_Wr);
		
		int flags = fcntl(pdata.hStd_OUT_Rd, F_GETFL, 0);
		if (flags == -1) {
			perror("fcntl");
			return 0;
		}
		
		flags |= O_NONBLOCK; // Add the O_NONBLOCK flag
		if (fcntl(pdata.hStd_OUT_Rd, F_SETFL, flags) == -1) {
			perror("fcntl");
			return 0;
		}
		
		pdata.hProcess = childPid;
		pdata.subpid = g_subpid++;
		g_process_data.push_back(pdata);
		return pdata.subpid;
	}
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

	int status = 0;
	pid_t result = waitpid(pdata->hProcess, &status, WNOHANG);

	if (result == 0) {
		return true;
	}

	return false;
}


bool readChildProcessStdout(int subpid, char* outputBuffer, int bytesWanted, int& bytesRead, bool block)
{
	ProcessData* pdata = findSubprocess(subpid);
	if (!pdata) {
		println("Invalid subprocess id %d", subpid);
		return false;
	}

	ssize_t nRead = 0;
	bytesRead = 0;
	
	while (bytesWanted) {
		nRead = read(pdata->hStd_OUT_Rd, outputBuffer + bytesRead, bytesWanted);

		if (nRead <= 0 && !block) {
			return false;
		}

		if (nRead == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
			//perror("read");
			return false;
		}
		
		if (nRead > 0) {
			bytesRead += nRead;
			bytesWanted -= nRead;
		}
		
		if (block && nRead == 0 && !isProcessAlive(subpid)) {
			return false;
		}
	}

	return false;
}

bool killChildProcess(int subpid) {
	ProcessData* pdata = findSubprocess(subpid);
	if (!pdata) {
		println("Invalid subprocess id %d", subpid);
		return false;
	}

	if (pdata->hProcess == -1) {
		return false;
	}

	if (!isProcessAlive(pdata->hProcess)) {
		close(pdata->hProcess); // Close the process descriptor
		close(pdata->hStd_OUT_Rd);
		pdata->hProcess = -1;
		return false;
	}

	int result = kill(pdata->hProcess, SIGKILL);
	if (result == -1) {
		perror("kill");
		println("Failed to kill pid %d", pdata->hProcess);
		return false;
	}

	close(pdata->hProcess); // Close the process descriptor
	close(pdata->hStd_OUT_Rd);
	pdata->hProcess = -1;

	return true;
}

void killAllChildren() {
	for (int i = 0; i < g_process_data.size(); i++) {
		killChildProcess(g_process_data[i].subpid);
	}

	g_process_data.clear();
}

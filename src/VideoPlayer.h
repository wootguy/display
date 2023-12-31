#pragma once
#include "mmlib.h"
#include <string>
#include "AudioPlayer.h"
#include "Display.h"

using namespace std;

#define MAX_QUEUE 8

struct Video {
	string python_output;
	map<string, string> keyavlues;
};

class VideoPlayer {
public:
	Display m_disp;
	int wantFps = 30;
	string lastUrl;
	AudioPlayer* audio_player;
	bool paused;
	float syncDelay = 0.35f; // time to delay video to keep in sync with audio

	vector<Video> videoQueue;

	VideoPlayer();
	~VideoPlayer();
	
	void loadNewVideo();

	void init();

	void play(std::string url);

	void stopVideo();

	void setMode(int bits, bool rgb, float fps);

	void restartVideo();

	void skipVideo();

	void loadNextQueueVideo();

	void think();

	void playNewVideo(int frameOffset);

	void unload();

	void pause();

private:
	EHandle loadingSpr;

	bool displayCreated = false;

	bool m_python_running = false;
	string m_python_output;

	bool m_video_downloading = false;
	bool m_video_playing = false;
	bool m_video_buffering = false;

	color24* frameData = NULL;

	int actualWidth = 42;
	int actualHeight = 22;

	int resizeBufferW = 0;
	int resizeBufferH = 0;
	color24* resizedFrameData = NULL;

	uint32_t pythonPid = 0;
	uint32_t decodePid = 0;
	uint32_t downloadPid = 0;

	int frameIdx = 0;

	int actualFps = 15;
	bool canFastReplay = false;

	float nextVideoPlay = 0;
	int nextPlayOffset = 0;

	float downloadStartTime = 0;
	float duration = 0;

	vector<EHandle> audio_areas;

	void convertFrame();

	void readFfmpegOutput(int subpid);

	void readPythonOutput(int subpid);

	void monitorVideoDownloadProcess(int subpid);

	void loadVideoInfo(std::string url);

	void updateSpeakerIdx();

	bool parsePythonOutput(string python_str, map<string, string>& outputMap);

	// find the best display resolution to fit the content
	// input is actual video size, which is scaled to the max supported by the display
	void sizeToFit(int width, int height);
};
#pragma once
#include "mmlib.h"
#include <string>
#include "AudioPlayer.h"
#include "Display.h"

using namespace std;

class VideoPlayer {
public:

	VideoPlayer();
	~VideoPlayer();
	
	void loadNewVideo();

	void init();

	void play(std::string url, float fps);

	void stopVideo();

	void think();

	void playNewVideo(int frameOffset);

private:
	Display m_disp;
	bool displayCreated = false;

	bool m_python_running = false;
	string m_python_output;

	bool m_video_downloading = false;
	bool m_video_playing = false;
	bool m_video_buffering = false;

	AudioPlayer* audio_player;

	color24* frameData = NULL;

	int actualWidth = 42;
	int actualHeight = 22;

	uint32_t pythonPid = 0;
	uint32_t decodePid = 0;
	uint32_t downloadPid = 0;

	int frameIdx = 0;

	float nextVideoPlay = 0;
	int nextPlayOffset = 0;

	float downloadStartTime = 0;
	float duration = 0;

	void convertFrame();

	void readFfmpegOutput(int subpid);

	void readPythonOutput(int subpid);

	void monitorVideoDownloadProcess(int subpid);

	void loadVideoInfo(std::string url);

	void updateSpeakerIdx();
};
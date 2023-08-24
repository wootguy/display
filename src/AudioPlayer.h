#pragma once
#include "mmlib.h"
#include <map>
#include <vector>
#include <string>
#include <thread>
#include "SteamVoiceEncoder.h"
#include "misc_utils.h"
#include "main.h"
#include "mp3.h"

const uint64_t steamid64_min = 0x0110000100000001;
const uint64_t steamid64_max = 0x01100001FFFFFFFF;

#define IDEAL_BUFFER_SIZE 8
#define MAX_SOUND_SAMPLE_RATE 22050
#define STREAM_BUFFER_SIZE 32768 // buffer size for file read bytes and sample rate conversion buffers

//#define SINGLE_THREAD_MODE // for easier debugging

struct VoicePacket {
	uint32_t size = 0;
	bool isNewSound = false; // true if start of a new sound

	std::vector<std::string> sdata;
	std::vector<uint32_t> ldata;
	std::vector<uint8_t> data;

	VoicePacket() {}
};

struct AudioPlayer {
public:
	ThreadSafeQueue<VoicePacket> outPackets[MAX_PLAYERS];
	ThreadSafeQueue<std::string> commands;
	ThreadSafeQueue<std::string> errors;
	
	// only write these vars from main thread
	volatile bool exitSignal = false;
	volatile bool stopSignal = false;
	uint32_t listeners = 0xffffffff; // 1 bit = player is listener
	uint32_t reliableMode = 0; // 1 bit = player wants reliable packets
	uint64_t playbackStartTime = 0;
	uint64_t nextPacketTime = 0;
	int packetNum = 0;
	Mp3DecodeState* mp3state = NULL;

	volatile int playerIdx; // TODO: mutex this?

	AudioPlayer(int playerIdx);
	~AudioPlayer();

	void playMp3(std::string mp3Path);

	void think(); // don't call directly unless in single thread mode

	void play_samples(); // only access from main thread

	void destroy();

	void stop();

	// progress in audio (slightly sped up)
	uint64_t getPlaybackTime();

private:
	// private vars only access from converter thread
	int sampleRate; // opus allows: 8, 12, 16, 24, 48 khz
	int frameDuration; // opus allows: 2.5, 5, 10, 20, 40, 60, 80, 100, 120
	int frameSize; // samples per frame
	int framesPerPacket; // 2 = minimum amount of frames for sven to accept packet
	int packetDelay; // millesconds between output packets
	int samplesPerPacket;
	int opusBitrate; // 32kbps = steam default

	// stream data from input wav file and do sample rate + pitch conversions
	// returns false for end of file
	bool read_samples();

	void write_output_packet(); // stream data from input wav file and write out a steam voice packet
	
	uint64_t steamid64 = steamid64_min;
	int pitch;
	int volume;
	std::thread* thinkThread = NULL;
	FILE* soundFile = NULL;

	uint8_t* readBuffer;
	float* rateBufferIn;
	float* rateBufferOut;
	bool startingNewSound;
	int16_t* volumeBuffer;
	
	vector<int16_t> encodeBuffer; // samples ready to be encoded
	SteamVoiceEncoder* encoder[MAX_PLAYERS];
};
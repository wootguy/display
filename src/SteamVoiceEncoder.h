#pragma once
#include <stdint.h>
#include <fstream>
#include <vector>
#include "opus/opus.h"

#define ENCODE_BUFFER_SIZE 4096

struct OpusFrame {
	uint16_t length;
	uint16_t sequence;
	uint8_t data[ENCODE_BUFFER_SIZE];
};

// must call crc32_init() before using this class
class SteamVoiceEncoder {
public:
	SteamVoiceEncoder(int frameSize, int framesPerPacket, int sampleRate, int bitrate, int encodeMode);

	~SteamVoiceEncoder();

	// sampleLen must be a multiple of (framesize*framesPerPacket)
	// returns size of packet, or -1 on failure
	std::vector<uint8_t> write_steam_voice_packet(int16_t* samples, int sampleLen, uint64_t steamid64);
	
	// resets encoder and sequence number for starting a new stream of audio (not necessary?)
	void reset();

	void finishTestFile();

	void updateEncoderSettings(int bitrate);

private:
	// samples must have frameSize length
	bool encode_opus_frame(int16_t* samples, OpusFrame& outFrame);

	int frameSize;
	int framesPerPacket;
	int sampleRate;
	int bitrate;
	uint16_t sequence; // clients use this to maintain the order of opus frames
	OpusEncoder* encoder;
	OpusFrame* frameBuffer;

	std::ofstream outFile;
};
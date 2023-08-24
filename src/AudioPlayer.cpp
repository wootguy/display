#include "AudioPlayer.h"
#include "misc_utils.h"
#include <chrono>
#include <thread>
#include <string.h>
#include "main.h"

#undef read
#undef write
#undef close

using namespace std;

float g_packet_delay = 0.05f;

AudioPlayer::AudioPlayer(int playerIdx) {
	sampleRate = 12000; // opus allows: 8, 12, 16, 24, 48 khz
	frameDuration = 10; // opus allows: 2.5, 5, 10, 20, 40, 60, 80, 100, 120
	frameSize = (sampleRate / 1000) * frameDuration; // samples per frame
	framesPerPacket = 5; // 2 = minimum amount of frames for sven to accept packet
	packetDelay = frameDuration * framesPerPacket; // millesconds between output packets
	samplesPerPacket = frameSize * framesPerPacket;
	opusBitrate = 32000; // 32kbps = steam default

	readBuffer = new uint8_t[STREAM_BUFFER_SIZE];
	rateBufferIn = new float[STREAM_BUFFER_SIZE];
	rateBufferOut = new float[STREAM_BUFFER_SIZE];
	volumeBuffer = new int16_t[samplesPerPacket];

	this->playerIdx = playerIdx;

	for (int i = 0; i < MAX_PLAYERS; i++) {
		encoder[i] = new SteamVoiceEncoder(frameSize, framesPerPacket, sampleRate, opusBitrate, OPUS_APPLICATION_AUDIO);
	}

	thinkThread = NULL;
	#ifndef SINGLE_THREAD_MODE
		thinkThread = new thread(&AudioPlayer::think, this);
	#endif
}

AudioPlayer::~AudioPlayer() {
	destroy();
}

void AudioPlayer::play_samples() {
	if (listeners == 0) {
		return;
	}

	if (playerIdx < 0 || playerIdx > gpGlobals->maxClients) {
		listeners = 0;
		return;
	}

	if (nextPacketTime > getEpochMillis()) {
		return;
	}

	static VoicePacket packets[MAX_PLAYERS];

	if (outPackets[0].size() == 0) {
		return;
	}

	outPackets[0].dequeue(packets[0]);

	if (packets[0].isNewSound) {
		packetNum = 0;
		playbackStartTime = getEpochMillis();
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		edict_t* plr = INDEXENT(i);
		uint32_t plrBit = 1 << (i & 31);
		VoicePacket& packet = packets[0];

		if (!isValidPlayer(plr)) {
			continue;
		}

		if ((listeners & plrBit) == 0) {
			continue;
		}

		if (packet.size == 0) {
			continue;
		}

		bool reliablePackets = reliableMode & plrBit;
		int sendMode = reliablePackets ? MSG_ONE : MSG_ONE_UNRELIABLE;

		// svc_voicedata
		MESSAGE_BEGIN(sendMode, 53, NULL, plr);
		WRITE_BYTE(playerIdx-1); // entity which is "speaking"
		WRITE_SHORT(packet.size); // compressed audio length

		// Ideally, packet data would be written once and re-sent to whoever wants it.
		// However there's no way to change the message destination after creation.
		// Data is combined into as few chunks as possible to minimize the loops
		// needed to write the data. This optimization loops about 10% as much as 
		// writing bytes one-by-one for every player.

		// First, data is split into strings delimted by zeros. It can't all be one string
		// because a string can't contain zeros, and the data is not guaranteed to end with a 0.
		for (int k = 0; k < packet.sdata.size(); k++) {
			WRITE_STRING(packet.sdata[k].c_str()); // includes the null terminater
		}

		// ...but that can leave a large chunk of bytes at the end, so the remainder is
		// also combined into 32bit ints.
		for (int k = 0; k < packet.ldata.size(); k++) {
			WRITE_LONG(packet.ldata[k]);
		}

		// whatever is left at this point will be less than 4 iterations.
		for (int k = 0; k < packet.data.size(); k++) {
			WRITE_BYTE(packet.data[k]);
		}

		MESSAGE_END();
	}

	packetNum++;
	nextPacketTime = playbackStartTime + (uint64_t)((float)(packetNum * (g_packet_delay - 0.0001f)) * 1000ULL); // slightly fast to prevent mic getting quiet/choppy
	//println("Play %d", packetNum);

	if (nextPacketTime < getEpochMillis()) {
		play_samples();
	}
}

uint64_t AudioPlayer::getPlaybackTime() {
	float speedup = g_packet_delay / (g_packet_delay - 0.0001f);
	return (getEpochMillis() - playbackStartTime) * speedup;
}

void AudioPlayer::destroy() {
	exitSignal = true;

	if (thinkThread) {
		thinkThread->join();
		delete thinkThread;
	}
	for (int i = 0; i < MAX_PLAYERS; i++) {
		delete encoder[i];
	}

	delete[] readBuffer;
	delete[] rateBufferIn;
	delete[] rateBufferOut;
	delete[] volumeBuffer;

	if (mp3state)
		delete mp3state;
}

void AudioPlayer::stop() {
	stopSignal = true;
}

void AudioPlayer::playMp3(string mp3Path) {
	commands.enqueue(mp3Path);
}

void AudioPlayer::think() {
	
#ifdef SINGLE_THREAD_MODE
	if (!exitSignal) {
#else
	while (!exitSignal) {
		this_thread::sleep_for(chrono::milliseconds(50));
#endif
		if (stopSignal) {
			destroyMp3(mp3state);
			mp3state = NULL;
			encodeBuffer.clear();
			for (int i = 0; i < MAX_PLAYERS; i++) {
				VoicePacket packet;
				while (outPackets[i].dequeue(packet));
				encoder[i]->reset();
			}
			stopSignal = false;
		}

		string mp3command;
		if (commands.dequeue(mp3command)) {
			destroyMp3(mp3state);
			mp3state = initMp3(mp3command);
			if (mp3state) {
				encodeBuffer.clear();
				startingNewSound = true;
				for (int i = 0; i < MAX_PLAYERS; i++)
					encoder[i]->reset();
			}
		}

		while (!exitSignal && (outPackets[0].size() < IDEAL_BUFFER_SIZE) && (mp3state || encodeBuffer.size())) {
			write_output_packet();
		}
	}
}

bool AudioPlayer::read_samples() {
	if (!mp3state) {
		return false;
	}

	if (!readMp3(mp3state, encodeBuffer, 0.7f)) {
		mp3state = NULL;
	}

	if (!mp3state) {
		return false;
	}

	return true;
}

void AudioPlayer::write_output_packet() {
	while (encodeBuffer.size() < samplesPerPacket && read_samples()) ;

	if (encodeBuffer.size() == 0) {
		//println("No data to encode");
		return; // no data to write
	}
	else if (encodeBuffer.size() < samplesPerPacket) {
		// pad the buffer to fill a packet
		//println("pad to fill opus packet");
		for (int i = encodeBuffer.size(); i < samplesPerPacket; i++) {
			encodeBuffer.push_back(0);
		}
	}
	
	for (int i = 0; i < gpGlobals->maxClients; i++) {
		vector<uint8_t> bytes;
		VoicePacket packet;
		packet.size = 0;

		if (startingNewSound) {
			packet.isNewSound = true;
			startingNewSound = false;
		}

		bytes = encoder[i]->write_steam_voice_packet(&encodeBuffer[0], samplesPerPacket, steamid64);

		string sdat = "";

		for (int x = 0; x < bytes.size(); x++) {
			byte bval = bytes[x];
			packet.data.push_back(bval);

			// combine into 32bit ints for faster writing later
			if (packet.data.size() == 4) {
				uint32 val = (packet.data[3] << 24) + (packet.data[2] << 16) + (packet.data[1] << 8) + packet.data[0];
				packet.ldata.push_back(val);
				packet.data.resize(0);
			}

			// combine into string for even faster writing later
			if (bval == 0) {
				packet.sdata.push_back(sdat);
				packet.ldata.resize(0);
				packet.data.resize(0);
				sdat = "";
			}
			else {
				sdat += (char)bval;
			}
		}

		packet.size = bytes.size();

		//println("Write %d samples with %d left. %d packets in queue", samplesPerPacket, encodeBuffer.size(), outPackets.size());

		outPackets[i].enqueue(packet);

		break;
	}

	encodeBuffer.erase(encodeBuffer.begin(), encodeBuffer.begin() + samplesPerPacket);
}


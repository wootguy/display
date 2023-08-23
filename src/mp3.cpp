#include "mp3.h"
#include "mmlib.h"
#include <stdio.h>
#include <vector>
#include <fstream>

// https://github.com/lieff/minimp3
#define MINIMP3_IMPLEMENTATION
//#define MINIMP3_ONLY_MP3
#include "minimp3.h"

#define MP3_BUFFER_SIZE 16384

struct Mp3DecodeState {
	mp3dec_t mp3d;
	int readPos;
	int readSize;
	int bufferLeft;
	int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	int16_t resampledPcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	uint8_t buffer[MP3_BUFFER_SIZE];
	FILE* file;
};

using namespace std;

int resamplePcm(int16_t* pcm_old, int16_t* pcm_new, int oldRate, int newRate, int numSamples) {
	float samplesPerStep = (float)oldRate / newRate;
	int numSamplesNew = (float)numSamples / samplesPerStep;
	float t = 0;

	for (int i = 0; i < numSamplesNew; i++) {
		int newIdx = t;
		pcm_new[i] = pcm_old[newIdx];
		t += samplesPerStep;
	}

	return numSamplesNew;
}

// mixes samples in-place without a new array
int mixStereoToMono(int16_t* pcm, int numSamples) {

	for (int i = 0; i < numSamples / 2; i++) {
		float left = ((float)pcm[i * 2] / 32768.0f);
		float right = ((float)pcm[i * 2 + 1] / 32768.0f);
		pcm[i] = clampf(left + right, -1.0f, 1.0f) * 32767;
	}

	return numSamples / 2;
}

void amplify(int16_t* pcm, int numSamples, double volume) {
	for (int i = 0; i < numSamples; i++) {
		double samp = ((double)pcm[i] / 32768.0f);
		pcm[i] = clampf(samp * volume, -1.0f, 1.0f) * 32767;
	}
}

Mp3DecodeState* initMp3(string fileName) {
	Mp3DecodeState* state = new Mp3DecodeState;
	memset(state, 0, sizeof(Mp3DecodeState));

	state->file = fopen(fileName.c_str(), "rb");
	if (!state->file) {
		fprintf(stderr, "Unable to open: %s\n", fileName.c_str());
		delete state;
		return NULL;
	}

	mp3dec_init(&state->mp3d);

	state->readPos = 0;
	state->readSize = MP3_BUFFER_SIZE;
	state->bufferLeft = 0;

	return state;
}

void destroyMp3(Mp3DecodeState* state) {
	if (!state) {
		return;
	}
	fclose(state->file);
	delete state;
}

bool readMp3(Mp3DecodeState* state, vector<int16_t>& outputSamples, float volume) {
	int readBytes = fread(state->buffer + state->readPos, 1, state->readSize, state->file);
	if (readBytes == 0 && state->bufferLeft == 0) {
		destroyMp3(state);
		return false;
	}
	state->bufferLeft += readBytes;

	mp3dec_frame_info_t info;
	int samples = mp3dec_decode_frame(&state->mp3d, state->buffer, state->bufferLeft, state->pcm, &info);
	samples *= info.channels;

	// remove the read bytes from the buffer
	int bytesRead = info.frame_bytes;
	memmove(state->buffer, state->buffer + bytesRead, MP3_BUFFER_SIZE - bytesRead);
	state->readSize = bytesRead;
	state->readPos = MP3_BUFFER_SIZE - bytesRead;
	state->bufferLeft -= bytesRead;

	if (info.channels == 2) {
		samples = mixStereoToMono(state->pcm, samples);
	}

	int writeSamples = samples;
	
	if (info.hz != 12000) {
		writeSamples = resamplePcm(state->pcm, state->resampledPcm, info.hz, 12000, samples);
	}
	else {
		memcpy(state->resampledPcm, state->pcm, samples * sizeof(int16_t));
	}

	if (volume != 1.0f) {
		amplify(state->resampledPcm, writeSamples, volume);
	}

	outputSamples.reserve(writeSamples);
	for (int i = 0; i < writeSamples; i++) {
		outputSamples.push_back(state->resampledPcm[i]);
	}

	return true;
}

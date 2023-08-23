#pragma once
#include <string>
#include <vector>

struct Mp3DecodeState;

Mp3DecodeState* initMp3(std::string fileName);

void destroyMp3(Mp3DecodeState* state);

bool readMp3(Mp3DecodeState* state, std::vector<int16_t>& outputSamples, float volume);
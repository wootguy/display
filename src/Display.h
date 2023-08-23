#pragma once
#include "mmlib.h"

using namespace std;

class ChunkModelConfig {
public:
	int chunkWidth;
	int chunkHeight;
	int numChunkModels;
	int skinsPerModel;
	int bodiesPerModel;

	int chunkSize;
	uint32_t numPixelCombinations;
	int bits; // bits per pixel

	string chunk_path;

	ChunkModelConfig() {}

	ChunkModelConfig(int chunkWidth, int chunkHeight, int bits, int numChunkModels, int skinsPerModel, int bodiesPerModel);
};

class DisplayConfig {
public:
	ChunkModelConfig chunk;
	int width;
	int height;
	int bits;
	bool rgb;

	string input_fname;

	DisplayConfig() {}

	DisplayConfig(ChunkModelConfig chunkConfig, int width, int height, bool rgb);
};

class Display {
public:
	int width, height;
	int chunkW, chunkH, chans;
	float scale;
	float fps = 15;
	Vector pos, angles;
	bool rgb_mode;
	EHandle light_red, light_green, light_blue, background;

	Vector up, right, forward;

	vector<vector<vector<EHandle>>> chunks;
	vector<uint32_t> g_num_to_chunk;

	int frameLoadProgress = 0;

	int frameCounter = 0;
	int lightFrameCounter = 0;
	Color lightColor;
	float videoStartTime = 0;

	// for converting pixel values to chunk values
	uint64_t chunkBits[256] = { 0 };
	vector<int> chunkModelIdx;

	Display() {}

	Display(Vector pos, Vector angles, uint32_t width, uint32_t height, float scale, bool rgb_mode);

	void orient(Vector pos, Vector angles);

	void loadNewVideo(string url);

	void createChunks();

	void loadFrame(string frameData);

	void setChunkValue(int x, int y, int chan, uint32_t value);

	void setLightValue(int r, int g, int b, int a);

	void destroy();

	void clear();
};

extern DisplayConfig display_cfg;
extern Display m_disp;
extern string g_quality;
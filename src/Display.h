#pragma once
#include "mmlib.h"

using namespace std;

#define MAX_CHUNKS (512-32) // save some space for player models before visible entity limit

class ChunkModelConfig {
public:
	int chunkWidth;
	int chunkHeight;
	int numChunkModels;
	int skinsPerModel;
	int bodiesPerModel;

	uint32_t numPixelCombinations;
	int bits; // bits per pixel

	string chunk_path;

	// for converting pixel values to chunk values
	vector<uint32_t> g_num_to_chunk;
	uint64_t chunkBits[256] = { 0 };
	vector<int> chunkModelIdx;

	ChunkModelConfig() {}

	ChunkModelConfig(int chunkWidth, int chunkHeight, int bits, int numChunkModels, int skinsPerModel, int bodiesPerModel);

	// generate lookup tables and model indexes 
	void init();
};

class Display {
public:
	int width, height; // pixels
	int chunkW; // number of chunks per row
	int chunkH; // number of chunks per column
	int chans;
	bool rgb;
	float scale;
	Vector pos, angles;
	EHandle background;
	ChunkModelConfig* cfg;
	bool dlight = true;

	Vector up, right, forward;

	vector<vector<vector<EHandle>>> chunks;

	Color lightColor;
	

	Display() {}

	Display(ChunkModelConfig* cfg, bool rgb);

	void orient(Vector pos, Vector angles, float scale);

	void createChunks();

	void setChunkValue(int x, int y, int chan, uint32_t value);

	void setLightValue(int r, int g, int b, int a);

	void resize(int width, int height);

	void setMode(int bits, bool rgb);

	void destroy();

	void clear();
};

extern string g_quality;
extern ChunkModelConfig g_chunk_configs[8];
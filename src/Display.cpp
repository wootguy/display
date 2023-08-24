#include "Display.h"

ChunkModelConfig g_chunk_configs[8] = {
	ChunkModelConfig(7, 3, 1, 1, 256, 256), // 1-bit
	ChunkModelConfig(5, 2, 2, 1, 256, 256), // 2-bit
	ChunkModelConfig(7, 1, 3, 1, 256, 256), // 3-bit
	ChunkModelConfig(5, 1, 4, 1, 256, 256), // 4-bit
	ChunkModelConfig(2, 2, 5, 1, 256, 256), // 5-bit
	ChunkModelConfig(3, 1, 6, 1, 100, 256), // 6-bit
	ChunkModelConfig(3, 1, 7, 1, 256, 256), // 7-bit
	ChunkModelConfig(2, 1, 8, 1, 256, 256), // 8-bit
};

string g_quality = "ld/";

ChunkModelConfig::ChunkModelConfig(int chunkWidth, int chunkHeight, int bits, int numChunkModels, int skinsPerModel, int bodiesPerModel) {
	this->chunkWidth = chunkWidth;
	this->chunkHeight = chunkHeight;
	this->numChunkModels = numChunkModels;
	this->skinsPerModel = skinsPerModel;
	this->bodiesPerModel = bodiesPerModel;
	this->bits = bits;

	int chunkSize = chunkWidth * chunkHeight;
	numPixelCombinations = int(pow(pow(2, bits), chunkSize));

	chunk_path = "models/display/" + to_string(bits) + "bit/";
}

void ChunkModelConfig::init() {
	int framesPerBody = 32;
	int framesLastBody = 32; // amunt of frames on the last body in a fully populated skin

	// map pixel values to chunk values
	int chunkSizeTotal = chunkWidth * chunkHeight;
	for (int i = 0; i < 255; i++) {
		uint64_t n = i + 1;
		uint64_t spread = 0;
		for (int b = 0; b < 64; b++) {
			uint64_t bit = 1LL << b;
			if (n & bit) {
				uint64_t chunkGap = chunkSizeTotal * b;
				//println("N has %d bit set and chunkGap is %d and b is %d", bit, chunkGap, b);
				spread |= 1LL << chunkGap;
			}
		}
		n = spread;
		chunkBits[i + 1] = n;
	}

	// map chunk values to entity settings
	uint32_t body = 0;
	uint32_t skin = 0;
	uint32_t model = 0;
	uint32_t frame = 0;
	g_num_to_chunk.resize(numPixelCombinations);
	for (uint32_t i = 0; i < numPixelCombinations; i++)
	{
		g_num_to_chunk[i] = (model << 24) + (skin << 16) + (body << 8) + frame;

		frame += 1;
		uint32_t maxFrames = body == 255 ? framesLastBody : framesPerBody;
		if (frame >= maxFrames)
		{
			frame = 0;
			body += 1;
			int maxBody = bodiesPerModel;
			if (body >= 256)
			{
				body = 0;
				skin += 1;
				if (int(skin) >= skinsPerModel)
				{
					skin = 0;
					model += 1;
					if (int(model) >= numChunkModels)
						model = 0;
				}
			}
		}
	}

	for (int i = 0; i < numChunkModels; i++) {
		string modelPath = chunk_path + g_quality + to_string(i) + ".mdl";
		chunkModelIdx.push_back(MODEL_INDEX(modelPath.c_str()));
	}
}

Display::Display(ChunkModelConfig* cfg, bool rgb)
{
	this->width = 0;
	this->height = 0;
	this->chunkW = 0;
	this->chunkH = 0;
	this->scale = scale;
	this->chans = rgb ? 3 : 1;
	this->rgb = rgb;
	this->cfg = cfg;
	this->scale = 1;

	ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("Created %d-bit display\n", cfg->bits));
}

void Display::orient(Vector newPos, Vector newAngles, float newScale)
{
	this->angles = newAngles;
	this->scale = newScale;
	this->pos = newPos;

	MAKE_VECTORS(angles);
	up = gpGlobals->v_up;
	right = gpGlobals->v_right;
	forward = gpGlobals->v_forward;

	Vector topLeft = pos + up * (chunkH / 2) * cfg->chunkHeight * scale
				    + -right * (chunkW / 2) * cfg->chunkWidth * scale;

	for (int c = 0; c < chans; c++)
	{
		for (int x = 0; x < chunkW; x++)
		{
			for (int y = 0; y < chunkH; y++)
			{
				Vector chunkPos = topLeft + right * x * cfg->chunkWidth * scale + -up * y * cfg->chunkHeight * scale;

				if (rgb) {
					chunkPos = chunkPos + (forward * c * -1) + forward * -0.5f;
				}
				else {
					chunkPos = chunkPos - forward * 4; // move into the white light
				}

				// round down to prevent tiny gaps between rows/columns
				chunkPos.x = (int)chunkPos.x;
				chunkPos.y = (int)chunkPos.y;

				CBaseEntity* chunk = chunks[c][x][y];
				if (!chunk) {
					return;
				}
				chunk->pev->origin = chunkPos;
				chunk->pev->scale = scale;
			}
		}
	}
}

void Display::resize(int width, int height) {
	if ((width % cfg->chunkWidth) != 0 || (height % cfg->chunkHeight) != 0) {
		println("Can't resize display to %dx%d. Not a multiple of chunk dimensions.", width, height);
		return;
	}

	int numChunks = (width / cfg->chunkWidth) * (height / cfg->chunkHeight) * chans;
	if (numChunks > MAX_CHUNKS) {
		println("Can't resize display to %dx%d. Too many chunks.", width, height);
		return;
	}

	destroy();

	this->width = width;
	this->height = height;

	chunkW = width / cfg->chunkWidth;
	chunkH = height / cfg->chunkHeight;

	float idealWidth = 448;
	float idealHeight = 256;
	scale = Min(idealWidth / width, idealHeight / height);
	scale = (int)scale; // round down to prevent tiny gaps between rows/columns
	println("Resized display to %dx%d, %.2f scale", width, height, scale);

	createChunks();
}

void Display::setMode(int bits, bool rgb) {
	if (bits < 1 || bits > 8) {
		println("Can't set mode. Invalid bits %d", bits);
		return;
	}

	destroy();

	this->rgb = rgb;
	chans = rgb ? 3 : 1;
	cfg = &g_chunk_configs[bits - 1];
}

void Display::clear() {
	if (!chunks.empty()) {
		for (int ch = 0; ch < chans; ch++) {
			for (int y = 0; y < chunkH; y++) {
				for (int x = 0; x < chunkW; x++) {
					setChunkValue(x, y, ch, 0);
				}
			}
		}
	}
	setLightValue(16, 16, 16, 0);
}

void Display::createChunks() {
	background = FIND_ENTITY_BY_TARGETNAME(NULL, "background");

	chunks.resize(chans);
	for (uint32_t c = 0; c < chunks.size(); c++)
	{
		chunks[c].resize(this->chunkW);
		for (int x = 0; x < chunkW; x++)
			chunks[c][x].resize(this->chunkH);
	}

	for (int c = 0; c < chans; c++)
	{
		map<string,string> ckeys;
		ckeys["model"] = cfg->chunk_path + g_quality + "0.mdl";
		ckeys["movetype"] = "5";
		ckeys["scale"] = to_string(scale);
		ckeys["targetname"] = "display_sprite";
		ckeys["angles"] = vecToString(angles + Vector(0, 180, 0));
		ckeys["rendermode"] = "0";
		ckeys["renderamt"] = "85";
		ckeys["targetname"] = "dchunk";

		for (int x = 0; x < chunkW; x++)
		{
			for (int y = 0; y < chunkH; y++)
			{
				ckeys["origin"] = vecToString(pos);
				
				CBaseEntity* spr = CreateEntity("item_generic", ckeys, true);
				spr->pev->solid = SOLID_NOT;
				chunks[c][x][y] = EHandle(spr);
			}
		}
	}

	orient(pos, angles, scale);
}

void Display::setChunkValue(int x, int y, int chan, uint32_t value) {
	if (x >= chunkW || y >= chunkH || chan >= chans) {
		println("Bad chunk idx %d/%d %d/%d %d/%d", x, chunkW, y, chunkH, chan, chans);
		return;
	}
	if (chunks.empty()) {
		println("Can't set chunk value. Display not initialized");
		return;
	}

	CBaseEntity* ent = chunks[chan][x][y];

	if (value >= cfg->numPixelCombinations) {
		return;
	}
	uint32_t c = cfg->g_num_to_chunk[value];
	uint32_t modelIdx = c >> 24;
	uint32_t skinIdx = (c >> 16) & 0xff;
	uint32_t bodyIdx = (c >> 8) & 0xff;
	uint32_t frameIdx = c & 0xff;

	ent->pev->model = cfg->chunkModelIdx[modelIdx];
	ent->pev->skin = skinIdx;
	ent->pev->body = bodyIdx;
	ent->pev->frame = frameIdx;
}

void Display::setLightValue(int r, int g, int b, int a) {	
	float sum = (r + g + b);
	if (sum == 0)
		sum = 1;
	bool r_on = (r / sum) >= 0.28;
	bool g_on = (g / sum) >= 0.28;
	bool b_on = (b / sum) >= 0.28;
	lightColor = Color(int(r * 0.1f), int(g * 0.1f), int(b * 0.1f));

	CBaseEntity* bgent = background;
	if (bgent) {
		bgent->pev->rendercolor = Vector(r, g, b);
	}

	Vector lightPos = pos + forward * scale * -256 + right * scale * width * 0.5f + up * scale * height * -0.5f;
	te_dlight(lightPos, 96, lightColor, 1, 0); // 0 causes too much flickering, 2 is too delayed
}

void Display::destroy() {
	for (int i = 0; i < chunks.size(); i++) {
		for (int k = 0; k < chunks[i].size(); k++) {
			for (int j = 0; j < chunks[i][k].size(); j++) {
				REMOVE_ENTITY(chunks[i][k][j]);
			}
		}
	}

	chunks.clear();
}
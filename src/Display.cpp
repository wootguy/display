#include "Display.h"

// 1bit 6x3
ChunkModelConfig chunk_cfg_1bit = ChunkModelConfig(11, 2, 1, 2, 256, 256);
ChunkModelConfig chunk_cfg_1bit_ld = ChunkModelConfig(6, 3, 1, 1, 256, 256);
DisplayConfig display_cfg_1bit_grey_ld = DisplayConfig(chunk_cfg_1bit_ld, 60, 30, false);
DisplayConfig display_cfg_1bit_rgb = DisplayConfig(chunk_cfg_1bit_ld, 72, 42, true);

// 2bit 3x3
ChunkModelConfig chunk_cfg_2bit_hd = ChunkModelConfig(11, 1, 2, 2, 256, 256);
ChunkModelConfig chunk_cfg_2bit_ld = ChunkModelConfig(5, 2, 2, 1, 256, 256);
DisplayConfig display_cfg_2bit_grey_ld = DisplayConfig(chunk_cfg_2bit_ld, 87, 51, false);
DisplayConfig display_cfg_2bit_grey_hd = DisplayConfig(chunk_cfg_2bit_hd, 87, 51, false);
DisplayConfig display_cfg_2bit_rgb_ld = DisplayConfig(chunk_cfg_2bit_ld, 51, 30, true);
DisplayConfig display_cfg_2bit_rgb_hd = DisplayConfig(chunk_cfg_2bit_hd, 51, 30, true);

// 3bit 3x2
ChunkModelConfig chunk_cfg_3bit_ld = ChunkModelConfig(7, 1, 3, 1, 256, 256);
ChunkModelConfig chunk_cfg_3bit_hd = ChunkModelConfig(4, 2, 3, 8, 256, 256);
DisplayConfig display_cfg_3bit_grey_hd = DisplayConfig(chunk_cfg_3bit_hd, 80, 48, false);
DisplayConfig display_cfg_3bit_grey_ld = DisplayConfig(chunk_cfg_3bit_ld, 77, 46, false);
DisplayConfig display_cfg_3bit_rgb_ld = DisplayConfig(chunk_cfg_3bit_ld, 42, 28, true);
DisplayConfig display_cfg_3bit_rgb_hd = DisplayConfig(chunk_cfg_3bit_hd, 48, 28, true);

// 2bit red, 3bit green, 1bit blue
DisplayConfig display_cfg_rgb_332 = DisplayConfig(chunk_cfg_3bit_ld, 48, 30, true);
// 16x15 = 240
// 16x10 = 160
// 8x10 = 80

// 4bit 2x2 (ineffecient)
ChunkModelConfig chunk_cfg_4bit_ld = ChunkModelConfig(5, 1, 4, 1, 256, 256);
ChunkModelConfig chunk_cfg_4bit_hd = ChunkModelConfig(3, 2, 4, 8, 256, 256);
DisplayConfig display_cfg_4bit_grey_ld = DisplayConfig(chunk_cfg_4bit_ld, 58, 32, false);
DisplayConfig display_cfg_4bit_grey_hd = DisplayConfig(chunk_cfg_4bit_hd, 58, 32, false);
DisplayConfig display_cfg_4bit_rgb_ld = DisplayConfig(chunk_cfg_4bit_ld, 34, 20, true);
DisplayConfig display_cfg_4bit_rgb_hd = DisplayConfig(chunk_cfg_4bit_hd, 16, 16, true);

// 5bit 3x1
ChunkModelConfig chunk_cfg_5bit_ld = ChunkModelConfig(2, 2, 5, 1, 256, 256);
DisplayConfig display_cfg_5bit_grey_ld = DisplayConfig(chunk_cfg_5bit_ld, 51, 30, false);
DisplayConfig display_cfg_5bit_rgb_ld = DisplayConfig(chunk_cfg_5bit_ld, 30, 16, true);

// 6bit 3x1
ChunkModelConfig chunk_cfg_6bit_ld = ChunkModelConfig(3, 1, 6, 11, 100, 256);
ChunkModelConfig chunk_cfg_6bit_hd = ChunkModelConfig(2, 2, 6, 11, 100, 256);
DisplayConfig display_cfg_6bit_grey_ld = DisplayConfig(chunk_cfg_6bit_ld, 51, 30, false);
DisplayConfig display_cfg_6bit_grey_hd = DisplayConfig(chunk_cfg_6bit_hd, 51, 30, false);
DisplayConfig display_cfg_6bit_rgb_ld = DisplayConfig(chunk_cfg_6bit_ld, 30, 16, true);

// 7bit
ChunkModelConfig chunk_cfg_7bit_ld = ChunkModelConfig(3, 1, 7, 1, 256, 256);
DisplayConfig display_cfg_7bit_grey_ld = DisplayConfig(chunk_cfg_7bit_ld, 51, 30, false);
DisplayConfig display_cfg_7bit_rgb_ld = DisplayConfig(chunk_cfg_7bit_ld, 30, 16, true);

//DisplayConfig display_cfg = display_cfg_3bit_rgb_ld;
DisplayConfig display_cfg = display_cfg_3bit_grey_ld;

string g_quality = "ld/";

DisplayConfig::DisplayConfig(ChunkModelConfig chunkConfig, int width, int height, bool rgb) {
	this->width = width;
	this->height = height;
	this->bits = chunkConfig.bits;
	this->rgb = rgb;

	this->chunk = chunkConfig;

	int bestWidth = 0;
	int bestHeight = 0;
	float idealAspect = 16.0f / 9.0f;
	int maxChunks = 512 - 32; // at least leave room for player models!
	int bestChunks = 0;
	float bestAspectDiff = 9e99;
	for (uint32_t y = 1; y < 128; y++) {
		for (uint32_t x = 1; x < 128; x++) {
			float aspectDiff = abs((float(x * chunkConfig.chunkWidth) / float(y * chunkConfig.chunkHeight)) - idealAspect);
			int numChunks = x * y * (rgb ? 3 : 1);
			if (numChunks > maxChunks) {
				break;
			}
			if (aspectDiff < bestAspectDiff || (aspectDiff < 0.5f && numChunks > bestChunks)) {
				bestChunks = numChunks;
				bestAspectDiff = aspectDiff;
				bestWidth = x * chunkConfig.chunkWidth;
				bestHeight = y * chunkConfig.chunkHeight;
			}
		}
	}
	float aspect = float(bestWidth) / float(bestHeight);
	//println("Best dims for %d-bit %s (chunk %dx%d): %dx%d (%f aspect, %d chunks)",
	//	bits, (rgb ? "RGB" : "GREY"), chunkConfig.chunkWidth, chunkConfig.chunkHeight, bestWidth, bestHeight, aspect, bestChunks);
	this->width = bestWidth;
	this->height = bestHeight;

	input_fname = "chunks_" + to_string(bits) + "bit";
	if (rgb)
		input_fname += "_rgb";
	input_fname += ".dat";
}

ChunkModelConfig::ChunkModelConfig(int chunkWidth, int chunkHeight, int bits, int numChunkModels, int skinsPerModel, int bodiesPerModel) {
	this->chunkWidth = chunkWidth;
	this->chunkHeight = chunkHeight;
	this->numChunkModels = numChunkModels;
	this->skinsPerModel = skinsPerModel;
	this->bodiesPerModel = bodiesPerModel;
	this->bits = bits;

	chunkSize = chunkWidth * chunkHeight;
	numPixelCombinations = int(pow(pow(2, bits), chunkSize));

	chunk_path = "models/display/" + to_string(bits) + "bit/";
}

Display::Display(Vector pos, Vector angles, uint32_t width, uint32_t height, float scale, bool rgb_mode)
{
	if (width % display_cfg.chunk.chunkWidth != 0 || (height % display_cfg.chunk.chunkHeight) != 0)
		println("Display size rounded to nearest multiple of %dx%d", display_cfg.chunk.chunkWidth, display_cfg.chunk.chunkHeight);
	this->width = width - (width % display_cfg.chunk.chunkWidth);
	this->height = height - (height % display_cfg.chunk.chunkHeight);
	this->chunkW = this->width / display_cfg.chunk.chunkWidth;
	this->chunkH = this->height / display_cfg.chunk.chunkHeight;
	this->scale = scale;
	this->rgb_mode = rgb_mode;
	this->chans = rgb_mode ? 3 : 1;

	chunks.resize(rgb_mode ? 3 : 1);
	for (uint32_t c = 0; c < chunks.size(); c++)
	{
		chunks[c].resize(this->chunkW);
		for (int x = 0; x < chunkW; x++)
			chunks[c][x].resize(this->chunkH);
	}

	this->scale = int((448.0f / height) * 0.5f);
	this->scale = 8; // for everything else
	//this->scale = 6; // for 1 bit greyscale
	println("SCALE SET TO %f", this->scale);

	int mapSize = int(pow(2, 4 * 4));

	int framesPerBody = 32;
	int framesLastBody = 32; // amunt of frames on the last body in a fully populated skin

	// map pixel values to chunk values
	
	int chunkSizeTotal = display_cfg.chunk.chunkWidth * display_cfg.chunk.chunkHeight;
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
	g_num_to_chunk.resize(display_cfg.chunk.numPixelCombinations);
	for (uint32_t i = 0; i < display_cfg.chunk.numPixelCombinations; i++)
	{
		g_num_to_chunk[i] = (model << 24) + (skin << 16) + (body << 8) + frame;

		frame += 1;
		uint32_t maxFrames = body == 255 ? framesLastBody : framesPerBody;
		if (frame >= maxFrames)
		{
			frame = 0;
			body += 1;
			int maxBody = display_cfg.chunk.bodiesPerModel;
			if (body >= 256)
			{
				body = 0;
				skin += 1;
				if (int(skin) >= display_cfg.chunk.skinsPerModel)
				{
					skin = 0;
					model += 1;
					if (int(model) >= display_cfg.chunk.numChunkModels)
						model = 0;
				}
			}
		}
	}

	string color_dir = rgb_mode ? g_quality : g_quality;
	for (int i = 0; i < display_cfg.chunk.numChunkModels; i++) {
		string modelPath = display_cfg.chunk.chunk_path + color_dir + to_string(i) + ".mdl";
		chunkModelIdx.push_back(MODEL_INDEX(modelPath.c_str()));
	}

	ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("Created %d-bit %dx%d display (%d ents)\n",
		display_cfg.bits, width, height, (chunkW * chunkH * chans)));

	orient(pos, angles);
}

void Display::orient(Vector pos, Vector angles)
{
	MAKE_VECTORS(angles);
	up = gpGlobals->v_up;
	right = gpGlobals->v_right;
	forward = gpGlobals->v_forward;

	this->pos = pos + up * (chunkH / 2) * display_cfg.chunk.chunkHeight * scale + -right * (chunkW / 2) * display_cfg.chunk.chunkWidth * scale;// + -forward*(8.0f/scale)*390;
	this->angles = angles;
}

void Display::loadNewVideo(string url)
{
	println("NOT IMPLEMENTED LOADNEWVIDEO");
	/*
	@sound_comms = g_FileSystem.OpenFile(sound_comms_path, OpenFile::WRITE);
	sound_comms.Write("load " + url + " " + display_cfg.bits + " " + display_cfg.rgb + " " +
		display_cfg.width + " " + display_cfg.height + " " + display_cfg.chunk.chunkWidth + " " + display_cfg.chunk.chunkHeight);
	sound_comms.Close();
	*/
}

void Display::clear() {
	for (int ch = 0; ch < chans; ch++) {
		for (int y = 0; y < chunkH; y++) {
			for (int x = 0; x < chunkW; x++) {
				setChunkValue(x, y, ch, 0);
			}
		}
	}
	setLightValue(16, 16, 16, 0);
}

void Display::createChunks() {
	light_red = FIND_ENTITY_BY_TARGETNAME(NULL, "light_red");
	light_green = FIND_ENTITY_BY_TARGETNAME(NULL, "light_green");
	light_blue = FIND_ENTITY_BY_TARGETNAME(NULL, "light_blue");
	//light_white = g_EntityFuncs.FindEntityByTargetname(NULL, "light_white");
	background = FIND_ENTITY_BY_TARGETNAME(NULL, "background");

	int channels = display_cfg.rgb ? 3 : 1;

	for (int c = 0; c < channels; c++)
	{
		string color_dir = rgb_mode ? g_quality : g_quality;

		map<string,string> ckeys;
		ckeys["model"] = display_cfg.chunk.chunk_path + color_dir + "0.mdl";
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
				Vector chunkPos = pos + right * x * display_cfg.chunk.chunkWidth * scale + -up * y * display_cfg.chunk.chunkHeight * scale;

				chunkPos = chunkPos + (forward * c * -1 + forward * -0.5f);
				//chunkPos = chunkPos + right*x*1 + up*y*-1;

				if (!rgb_mode) {
					chunkPos = chunkPos - forward * 4 * 1; // move into the white light
				}

				ckeys["origin"] = vecToString(chunkPos);
				
				CBaseEntity* spr = CreateEntity("item_generic", ckeys, true);
				spr->pev->solid = SOLID_NOT;
				//spr->pev->rendermode = 5;

				chunks[c][x][y] = EHandle(spr);
			}
		}
	}
}

void Display::setChunkValue(int x, int y, int chan, uint32_t value) {
	if (x >= chunkW || y >= chunkH || chan >= chans) {
		println("Bad chunk idx %d/%d %d/%d %d/%d", x, chunkW, y, chunkH, chan, chans);
		return;
	}

	CBaseEntity* ent = chunks[chan][x][y];
	string color_dir = rgb_mode ? g_quality : g_quality;

	if (value >= display_cfg.chunk.numPixelCombinations) {
		return;
	}
	uint32_t c = g_num_to_chunk[value];
	uint32_t modelIdx = c >> 24;
	uint32_t skinIdx = (c >> 16) & 0xff;
	uint32_t bodyIdx = (c >> 8) & 0xff;
	uint32_t frameIdx = c & 0xff;

	ent->pev->model = chunkModelIdx[modelIdx];
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

void Display::loadFrame(string frameData)
{
	vector<string> chunkValues = splitString(frameData, " ");
	frameLoadProgress = 0;
	frameData = "";
	int chunkTotal = chunkW * chunkH;

	if (chunkTotal * chans >= int(chunkValues.size()))
	{
		println("Frame data does not match screen size %d %d", chunkValues.size(), (chunkW * chunkH));
		//g_Scheduler.SetTimeout("inc_delay", 0.05, this);
		return;
	}

	int soundIdx = 0;
	for (int ch = 0; ch < chans; ch++)
	{
		string color_dir = rgb_mode ? g_quality : g_quality;
		for (int y = 0; y < chunkH; y++)
		{
			for (int x = 0; x < chunkW; x++)
			{
				CBaseEntity* ent = chunks[ch][x][y];

				uint32_t value = atoi(chunkValues[chunkTotal * ch + y * chunkW + x].c_str());
				setChunkValue(x, y, ch, value);
			}
		}
	}

	uint32_t lighting = atoi(chunkValues[chunkValues.size() - 1].c_str());
	uint32_t r = (lighting >> 24) & 0xff;
	uint32_t g = (lighting >> 16) & 0xff;
	uint32_t b = (lighting >> 8) & 0xff;
	uint32_t a = lighting & 0xff;
	float sum = (r + g + b);
	if (sum == 0)
		sum = 1;
	bool r_on = (r / sum) >= 0.28;
	bool g_on = (g / sum) >= 0.28;
	bool b_on = (b / sum) >= 0.28;
	lightColor = Color(int(r * 0.1f), int(g * 0.1f), int(b * 0.1f));
	background.GetEntity()->pev->rendercolor = Vector(r, g, b);

	//light_red.GetEntity().Use(light_red, light_red, r_on ? USE_ON : USE_OFF);
	//light_green.GetEntity().Use(light_green, light_green, g_on ? USE_ON : USE_OFF);
	//light_blue.GetEntity().Use(light_blue, light_blue, b_on ? USE_ON : USE_OFF);
	//light_white.GetEntity().Use(light_white, light_white, w_on ? USE_ON : USE_OFF);

	Vector lightPos = pos + forward * scale * -256 + right * scale * width * 0.5f + up * scale * height * -0.5f;
	te_dlight(lightPos, 96, lightColor, 1, 0); // 0 causes too much flickering, 2 is too delayed
	//println("LIGHT DATA: " + lightColor.ToString());

	//println("Loaded frame " + frameCounter);
	//g_Scheduler.SetTimeout("inc_delay", 0.06666667, this);
	//g_Scheduler.SetTimeout("inc_delay", 0.00, this);
}

void Display::destroy() {
	for (int i = 0; i < chunks.size(); i++) {
		for (int k = 0; k < chunks[i].size(); k++) {
			for (int j = 0; j < chunks[i][k].size(); j++) {
				REMOVE_ENTITY(chunks[i][k][j]);
			}
		}
	}
}
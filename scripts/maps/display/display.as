
// Limits:
// Sprites - 256 frames max
// Models - 256 submodels max, 100 textures max
// Frames - not wise to go above 256 polies per model (100k epoly)
// GL_MAXTEXTURES at 4096 textures (~40 models)
// 1x19 can't work with 4x pixel size because of 1024 tex size limit
// main limit is the texture size. I can't use all 256 bodies and 256+frames if there aren't enough combos in a single tex

class DisplayConfig
{
	ChunkModelConfig chunk;
	int width;
	int height;
	int bits;
	bool rgb;
	
	string input_fname;
	
	DisplayConfig() {}
	
	DisplayConfig(ChunkModelConfig chunkConfig, int width, int height, bool rgb) {
		this.width = width;
		this.height = height;
		this.bits = chunkConfig.bits;
		this.rgb = rgb;
		
		this.chunk = chunkConfig;
		
		int bestWidth = 0;
		int bestHeight = 0;
		float idealAspect = 16.0f/9.0f;
		int maxChunks = 512 - 32; // at least leave room for player models!
		int bestChunks = 0;
		float bestAspectDiff = 9e99;
		for (uint y = 1; y < 128; y++) {
			for (uint x = 1; x < 128; x++) {
				float aspectDiff = abs((float(x*chunkConfig.chunkWidth) / float(y*chunkConfig.chunkHeight)) - idealAspect);
				int numChunks = x*y*(rgb ? 3 : 1);
				if (numChunks > maxChunks) {
					break;
				}
				if (aspectDiff < bestAspectDiff or (aspectDiff < 0.5f and numChunks > bestChunks)) {
					bestChunks = numChunks;
					bestAspectDiff = aspectDiff;
					bestWidth = x*chunkConfig.chunkWidth;
					bestHeight = y*chunkConfig.chunkHeight;
				}
			}
		}
		float aspect = float(bestWidth)/float(bestHeight);
		println("Best dims for " + bits + "-bit " + (rgb ? "RGB" : "GREY") + 
			" (chunk " + chunkConfig.chunkWidth + "x" + chunkConfig.chunkHeight + "): " + 
			bestWidth + "x" + bestHeight + " (" + (aspect) + " aspect, " + bestChunks + " chunks");
		this.width = bestWidth;
		this.height = bestHeight;
			
		input_fname = "chunks_" + bits + "bit";
		if (rgb)
			input_fname += "_rgb";
		input_fname += ".dat";
	}
}

class ChunkModelConfig
{
	int chunkWidth;
	int chunkHeight;
	int numChunkModels;
	int skinsPerModel;
	int bodiesPerModel;
	
	int chunkSize;
	uint numPixelCombinations;
	int bits; // bits per pixel
	
	string chunk_path;
	
	ChunkModelConfig() {}
	
	ChunkModelConfig(int chunkWidth, int chunkHeight, int bits, int numChunkModels, int skinsPerModel, int bodiesPerModel) {
		this.chunkWidth = chunkWidth;
		this.chunkHeight = chunkHeight;
		this.numChunkModels = numChunkModels;
		this.skinsPerModel = skinsPerModel;
		this.bodiesPerModel = bodiesPerModel;
		this.bits = bits;
		
		chunkSize = chunkWidth * chunkHeight;
		numPixelCombinations = int( pow(pow(2,bits), chunkSize) );
		
		chunk_path = "models/display/" + bits + "bit/";
	}
}

// 1bit 6x3
ChunkModelConfig chunk_cfg_1bit = ChunkModelConfig(11, 2, 1, 2, 256, 256);
ChunkModelConfig chunk_cfg_1bit_ld = ChunkModelConfig(6, 3, 1, 1, 256, 256);
DisplayConfig display_cfg_1bit_grey_ld = DisplayConfig(chunk_cfg_1bit_ld, 60, 30, false);
DisplayConfig display_cfg_1bit_rgb  = DisplayConfig(chunk_cfg_1bit_ld, 72, 42, true);

// 2bit 3x3
ChunkModelConfig chunk_cfg_2bit_hd = ChunkModelConfig(11, 1, 2, 2, 256, 256);
ChunkModelConfig chunk_cfg_2bit_ld = ChunkModelConfig(5, 2, 2, 1, 256, 256);
DisplayConfig display_cfg_2bit_grey_ld = DisplayConfig(chunk_cfg_2bit_ld, 87, 51, false);
DisplayConfig display_cfg_2bit_grey_hd = DisplayConfig(chunk_cfg_2bit_hd, 87, 51, false);
DisplayConfig display_cfg_2bit_rgb_ld  = DisplayConfig(chunk_cfg_2bit_ld, 51, 30, true);
DisplayConfig display_cfg_2bit_rgb_hd  = DisplayConfig(chunk_cfg_2bit_hd, 51, 30, true);

// 3bit 3x2
ChunkModelConfig chunk_cfg_3bit_ld = ChunkModelConfig(7, 1, 3, 1, 256, 256);
ChunkModelConfig chunk_cfg_3bit_hd = ChunkModelConfig(4, 2, 3, 8, 256, 256);
DisplayConfig display_cfg_3bit_grey_hd = DisplayConfig(chunk_cfg_3bit_hd, 80, 48, false);
DisplayConfig display_cfg_3bit_grey_ld = DisplayConfig(chunk_cfg_3bit_ld, 77, 46, false);
DisplayConfig display_cfg_3bit_rgb_ld  = DisplayConfig(chunk_cfg_3bit_ld, 42, 28, true);
DisplayConfig display_cfg_3bit_rgb_hd  = DisplayConfig(chunk_cfg_3bit_hd, 48, 28, true);

// 2bit red, 3bit green, 1bit blue
DisplayConfig display_cfg_rgb_332  = DisplayConfig(chunk_cfg_3bit_ld, 48, 30, true);
// 16x15 = 240
// 16x10 = 160
// 8x10 = 80

// 4bit 2x2 (ineffecient)
ChunkModelConfig chunk_cfg_4bit_ld = ChunkModelConfig(5, 1, 4, 1, 256, 256);
ChunkModelConfig chunk_cfg_4bit_hd = ChunkModelConfig(3, 2, 4, 8, 256, 256);
DisplayConfig display_cfg_4bit_grey_ld = DisplayConfig(chunk_cfg_4bit_ld, 58, 32, false);
DisplayConfig display_cfg_4bit_grey_hd = DisplayConfig(chunk_cfg_4bit_hd, 58, 32, false);
DisplayConfig display_cfg_4bit_rgb_ld  = DisplayConfig(chunk_cfg_4bit_ld, 34, 20, true);
DisplayConfig display_cfg_4bit_rgb_hd  = DisplayConfig(chunk_cfg_4bit_hd, 16, 16, true);

// 5bit 3x1
ChunkModelConfig chunk_cfg_5bit_ld = ChunkModelConfig(2, 2, 5, 1, 256, 256);
DisplayConfig display_cfg_5bit_grey_ld = DisplayConfig(chunk_cfg_5bit_ld, 51, 30, false);
DisplayConfig display_cfg_5bit_rgb_ld  = DisplayConfig(chunk_cfg_5bit_ld, 30, 16, true);

// 6bit 3x1
ChunkModelConfig chunk_cfg_6bit = ChunkModelConfig(3, 1, 6, 11, 100, 256);
DisplayConfig display_cfg_6bit_grey = DisplayConfig(chunk_cfg_6bit, 51, 30, false);
DisplayConfig display_cfg_6bit_rgb  = DisplayConfig(chunk_cfg_6bit, 30, 16, true);

// 7bit
ChunkModelConfig chunk_cfg_7bit_ld = ChunkModelConfig(3, 1, 7, 1, 256, 256);
DisplayConfig display_cfg_7bit_grey_ld = DisplayConfig(chunk_cfg_7bit_ld, 51, 30, false);
DisplayConfig display_cfg_7bit_rgb_ld  = DisplayConfig(chunk_cfg_7bit_ld, 30, 16, true);

// 8bit 2x1 (ineffecient)
ChunkModelConfig chunk_cfg_8bit = ChunkModelConfig(2, 1, 8, 3, 100, 256);
ChunkModelConfig chunk_cfg_8bit_hd = ChunkModelConfig(3, 1, 8, 8, 256, 256);
DisplayConfig display_cfg_8bit_grey = DisplayConfig(chunk_cfg_8bit, 42, 24, false);
DisplayConfig display_cfg_8bit_rgb  = DisplayConfig(chunk_cfg_8bit, 24, 14, true);
DisplayConfig display_cfg_8bit_rgb_hd  = DisplayConfig(chunk_cfg_8bit_hd, 24, 14, true);


string g_quality = "ld/";

//
// ~~~~~~~~~~~~~~~~~~~ CHOOSE CONFIG HERE ~~~~~~~~~~~~~~~~~~~
//
DisplayConfig display_cfg = display_cfg_7bit_grey_ld;
//
// ~~~~~~~~~~~~~~~~~~~ CHOOSE CONFIG HERE ~~~~~~~~~~~~~~~~~~~
//


Display g_disp = Display(Vector(0,0,0), Vector(0,0,0), display_cfg.width, display_cfg.height, 4, display_cfg.rgb);	

// 1x19
/*
int numChunkModels = 21;
int chunkSize = 19;
int chunkWidth = 19;
int chunkHeight = 1;
*/


void print(string text) { g_Game.AlertMessage( at_console, text); }
void println(string text) { print(text + "\n"); }

void inc_delay(Display disp)
{
	if (disp is null)
		return;
	//disp.inc();
	//disp.loadFrame();
	g_disp.loadFrame();
}

array<uint> g_num_to_chunk; // converts binary number to chunk details (model + body + skin)

CBasePlayer@ getAnyPlayer() 
{
	CBaseEntity@ ent = null;
	do {
		@ent = g_EntityFuncs.FindEntityByClassname(ent, "player");
		if (ent !is null) {
			CBasePlayer@ plr = cast<CBasePlayer@>(ent);
			return plr;
		}
	} while (ent !is null);
	return null;
}

void sayGlobal(string text) {
	g_PlayerFuncs.SayTextAll(getAnyPlayer(), text +"\n");
}

void init()
{
	int mapSize = int( pow(2, 4*4) );
	
	int framesPerBody = 32;
	int framesLastBody = 32; // amunt of frames on the last body in a fully populated skin
	
	uint body = 0;
	uint skin = 0;
	uint model = 0;
	uint frame = 0;
	g_num_to_chunk.resize(display_cfg.chunk.numPixelCombinations);
	for (uint i = 0; i < display_cfg.chunk.numPixelCombinations; i++)
	{
		g_num_to_chunk[i] = (model << 24) + (skin << 16) + (body << 8) + frame;
		
		frame += 1;
		uint maxFrames = body == 255 ? framesLastBody : framesPerBody;
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
}

string sound_comms_path = "scripts/maps/temp/sound_comms";
string sound_comms_path2 = "scripts/maps/temp/sound_comms2";

class Color
{ 
	uint8 r, g, b, a;
	
	Color() { r = g = b = a = 0; }
	Color(uint8 _r, uint8 _g, uint8 _b, uint8 _a = 255 ) { r = _r; g = _g; b = _b; a = _a; }
	Color (Vector v) { r = int(v.x); g = int(v.y); b = int(v.z); a = 255; }
	string ToString() { return "" + r + " " + g + " " + b + " " + a; }
}

void te_dlight(Vector pos, uint8 radius=32, Color c=PURPLE, 
	uint8 life=255, uint8 decayRate=50,
	NetworkMessageDest msgType=MSG_BROADCAST, edict_t@ dest=null)
{
	NetworkMessage m(msgType, NetworkMessages::SVC_TEMPENTITY, dest);
	m.WriteByte(TE_DLIGHT);
	m.WriteCoord(pos.x);
	m.WriteCoord(pos.y);
	m.WriteCoord(pos.z);
	m.WriteByte(radius);
	m.WriteByte(c.r);
	m.WriteByte(c.g);
	m.WriteByte(c.b);
	m.WriteByte(life);
	m.WriteByte(decayRate);
	m.End();
}

class Display
{
	int width, height;
	int chunkW, chunkH, chans;
	float scale;
	float fps = 30.0f;
	Vector pos, angles;
	bool rgb_mode;
	EHandle light_red, light_green, light_blue, background;
	
	Vector up, right, forward;
	
	array<array<array<EHandle>>> chunks;
	
	int frameLoadProgress = 0;
	string frameData;
	
	File@ f = null;
	File@ audioFile = null;
	int frameCounter = 0;
	int lightFrameCounter = 0;
	Color lightColor;
	float videoStartTime = 0;
	
	File@ sound_comms = null;
	
	Display() {}
	
	Display(Vector pos, Vector angles, uint width, uint height, float scale, bool rgb_mode)
	{
		if (width % display_cfg.chunk.chunkWidth != 0 or height % display_cfg.chunk.chunkHeight != 0)
			println("Display size rounded to nearest multiple of " + display_cfg.chunk.chunkWidth + "x" + display_cfg.chunk.chunkHeight);
		this.width = width - (width % display_cfg.chunk.chunkWidth);
		this.height = height - (height % display_cfg.chunk.chunkHeight);
		this.chunkW = this.width / display_cfg.chunk.chunkWidth;
		this.chunkH = this.height / display_cfg.chunk.chunkHeight;
		this.scale = scale;
		this.rgb_mode = rgb_mode;
		this.chans = rgb_mode ? 3 : 1;
		
		chunks.resize(rgb_mode ? 3 : 1);
		for (uint c = 0; c < chunks.size(); c++)
		{
			chunks[c].resize(this.chunkW);
			for (int x = 0; x < chunkW; x++)
				chunks[c][x].resize(this.chunkH);
		}
		
		this.scale = int((448.0f / height)*0.5f);
		//this.scale = 8; // for everything else
		this.scale = 6; // for 1 bit greyscale
		println("SCALE SET TO " + this.scale);
			
		println("Created " + chunkW + "x" + chunkH + " display (" + (chunkW*chunkH) + " ents)");
		
		orient(pos, angles);
	}
	
	void orient(Vector pos, Vector angles)
	{
		g_EngineFuncs.MakeVectors(angles);
		up = g_Engine.v_up;
		right = g_Engine.v_right;
		forward = g_Engine.v_forward;
		
		this.pos = pos + up*(chunkH/2)*display_cfg.chunk.chunkHeight*scale + -right*(chunkW/2)*display_cfg.chunk.chunkWidth*scale + -forward*(8.0f/scale)*390;
		this.angles = angles;
	}
	
	void loadNewVideo(string url)
	{
		@sound_comms = g_FileSystem.OpenFile( sound_comms_path, OpenFile::WRITE);
		sound_comms.Write("load " + url + " " + display_cfg.bits + " " + display_cfg.rgb + " " + 
			display_cfg.width + " " + display_cfg.height + " " + display_cfg.chunk.chunkWidth + " " + display_cfg.chunk.chunkHeight);
		sound_comms.Close();
	}
	
	bool lolwut = false;
	
	void createChunks()
	{
		light_red = g_EntityFuncs.FindEntityByTargetname(null, "light_red");
		light_green = g_EntityFuncs.FindEntityByTargetname(null, "light_green");
		light_blue = g_EntityFuncs.FindEntityByTargetname(null, "light_blue");
		//light_white = g_EntityFuncs.FindEntityByTargetname(null, "light_white");
		background = g_EntityFuncs.FindEntityByTargetname(null, "background");
		
		int channels = display_cfg.rgb ? 3 : 1;
		
		lolwut = !lolwut;
		
		for (int c = 0; c < channels; c++)
		{
			string color_dir = rgb_mode ? g_quality : g_quality;
			
			dictionary ckeys;
			ckeys["model"] = display_cfg.chunk.chunk_path + color_dir + "0.mdl";
			ckeys["movetype"] = "5";
			ckeys["scale"] = "" + scale;
			ckeys["targetname"] = "display_sprite";
			ckeys["angles"] = (angles + Vector(0,180,0)).ToString();
			ckeys["rendermode"] = "0";
			ckeys["renderamt"] = "85";
			ckeys["targetname"] = "dchunk";
			
			for (int x = 0; x < chunkW; x++)
			{
				for (int y = 0; y < chunkH; y++)
				{
					Vector chunkPos = pos + right*x*display_cfg.chunk.chunkWidth*scale + -up*y*display_cfg.chunk.chunkHeight*scale;
					
					chunkPos = chunkPos + (forward*c*-1 + forward*-0.5f);
					//chunkPos = chunkPos + right*x*1 + up*y*-1;
					
					if (!rgb_mode) {
						chunkPos = chunkPos - forward*4*1; // move into the white light
					}
					
					ckeys["origin"] = chunkPos.ToString();
					CBaseEntity@ spr = g_EntityFuncs.CreateEntity("item_generic", ckeys, true);
					spr.pev.solid = SOLID_NOT;
					//spr.pev.rendermode = 5;
					
					chunks[c][x][y] = EHandle(spr);
				}
			}
		}
		
		g_Scheduler.SetTimeout("inc_delay", 0.05, this);
	}
	
	void loadChunks(string fname, float fps)
	{
		display_cfg.input_fname = fname;
		this.fps = fps;
		@f = null;
	}
	
	void loadFrame()
	{
		if (f is null) {
			println("LOAD AGAIN");
			string fpath = "scripts/maps/display/" + display_cfg.input_fname;
			@f = g_FileSystem.OpenFile( fpath, OpenFile::READ );
			if( f is null or !f.IsOpen())
			{
				println("Failed to open " + fpath);
				g_Scheduler.SetTimeout("inc_delay", 1.0, this);
				return;
			}
			
			fpath = "scripts/maps/display/audio.dat";
			@audioFile = g_FileSystem.OpenFile( fpath, OpenFile::READ );
			if( audioFile is null or !audioFile.IsOpen())
			{
				println("Failed to open " + fpath);
				g_Scheduler.SetTimeout("inc_delay", 1.0, this);
				return;
			}
			
			string line;
			f.ReadLine(line);
			array<string> info = line.Split(" ");
			int bits = atoi(info[0]);
			bool greyscale = atoi(info[1]) != 0;
			int frameWidth = atoi(info[2]);
			int frameHeight = atoi(info[3]);
			int chunkWidth = atoi(info[4]);
			int chunkHeight = atoi(info[5]);
			float framerate = atof(info[6]);
			this.fps = Math.min(15, framerate);
			int chunks = (frameWidth/chunkWidth)*(frameHeight/chunkHeight)*(greyscale ? 1 : 3);
			
			
			sayGlobal("Playing " + bits + "-bit " + (greyscale ? "Greyscale " : "RGB ") + frameWidth + "x" + frameHeight 
				+ " video at " + framerate + " FPS (" + chunks + " chunks)");
			
			// start the sound
			@sound_comms = g_FileSystem.OpenFile( sound_comms_path, OpenFile::WRITE);
			sound_comms.Write("play");
			sound_comms.Close();
			
			videoStartTime = g_Engine.time;
			
			frameCounter = 0;
			lightFrameCounter = 0;
		}
		
		//fps = 2;
		float targetFrame = (g_Engine.time - videoStartTime) * fps;
		float targetLightFrame = (g_Engine.time - videoStartTime) * 30.0f;
		//println("" + frameCounter + " " + targetFrame);	
		
		string line;
		string audioLine;
		if (frameCounter >= targetFrame and frameLoadProgress < 4 and !f.EOFReached()) {
			f.ReadLine(line);
			frameData += line;
			frameLoadProgress++;
		}
		while( !f.EOFReached() and frameCounter < targetFrame )
		{
			while (frameLoadProgress < 4) {
				f.ReadLine(line);
				frameData += line;
				frameLoadProgress++;
			}
			
			audioFile.ReadLine(audioLine);
			
			if (++frameCounter < int(targetFrame))
				continue;
			
			array<string> chunkValues = frameData.Split(" ");
			array<string> audioValues = audioLine.Split(" ");
			frameLoadProgress = 0;
			frameData = "";
			int chunkTotal = chunkW*chunkH;
			
			if (chunkTotal*chans >= int(chunkValues.size()))
			{
				println("Frame data does not match screen size " + chunkValues.size() + " " + (chunkW*chunkH));
				@f = null;
				g_Scheduler.SetTimeout("inc_delay", 0.05, this);
				return;
			}
			
			lolwut = !lolwut;
			int soundIdx = 0;
			for (int ch = 0; ch < chans; ch++)
			{
				string color_dir = rgb_mode ? g_quality : g_quality;
				for (int y = 0; y < chunkH; y++)
				{
					for (int x = 0; x < chunkW; x++)
					{
						CBaseEntity@ ent = chunks[ch][x][y];
						
						/*
						if (soundIdx < 16 and soundIdx < int(audioValues.size())) {
							float vol = atof(audioValues[soundIdx]);
							//vol = (soundIdx / 32.0f);
							if (vol > 0)
							{
								g_SoundSystem.PlaySound(ent.edict(), lolwut ? CHAN_BODY : CHAN_BODY, "display/" + (soundIdx) + ".wav", vol, 0.0f, 0, 100);
							}
						}
						soundIdx++;
						*/
						
						uint value = atoi(chunkValues[chunkTotal*ch + y*chunkW + x]);
						if (value >= display_cfg.chunk.numPixelCombinations) {
							continue;
						}
						uint c = g_num_to_chunk[value];
						uint modelIdx = c >> 24;
						uint skinIdx = (c >> 16) & 0xff;
						uint bodyIdx = (c >> 8) & 0xff;
						uint frameIdx = c & 0xff;
						
						g_EntityFuncs.SetModel(ent, display_cfg.chunk.chunk_path + color_dir + modelIdx + ".mdl");
						ent.pev.skin = skinIdx;
						ent.pev.body = bodyIdx;
						ent.pev.frame = frameIdx;
						//ent.pev.origin.z -= 0.1f;
					}
				}
			}
			
			uint lighting = atoi(chunkValues[chunkValues.size()-1]);
			uint r = (lighting >> 24) & 0xff;
			uint g = (lighting >> 16) & 0xff;
			uint b = (lighting >> 8) & 0xff;
			uint a = lighting & 0xff;
			float sum = (r + g + b);
			if (sum == 0)
				sum = 1;
			bool r_on = (r / sum) >= 0.28;
			bool g_on = (g / sum) >= 0.28;
			bool b_on = (b / sum) >= 0.28;
			lightColor = Color(int(r*0.1f), int(g*0.1f), int(b*0.1f));
			background.GetEntity().pev.rendercolor = Vector(r, g, b);
			
			//light_red.GetEntity().Use(light_red, light_red, r_on ? USE_ON : USE_OFF);
			//light_green.GetEntity().Use(light_green, light_green, g_on ? USE_ON : USE_OFF);
			//light_blue.GetEntity().Use(light_blue, light_blue, b_on ? USE_ON : USE_OFF);
			//light_white.GetEntity().Use(light_white, light_white, w_on ? USE_ON : USE_OFF);
		}
		
		while (lightFrameCounter < targetLightFrame)
		{
			if (++lightFrameCounter < int(targetLightFrame))
				continue;
			Vector lightPos = pos + forward*scale*-256 + right*scale*width*0.5f + up*scale*height*-0.5f;
			te_dlight(lightPos, 96, lightColor, 1, 0); // 0 causes too much flickering, 2 is too delayed
			//println("LIGHT DATA: " + lightColor.ToString());
		}
		
 		//println("Loaded frame " + frameCounter);
		//g_Scheduler.SetTimeout("inc_delay", 0.06666667, this);
		g_Scheduler.SetTimeout("inc_delay", 0.00, this);
	}
	
	void inc()
	{
		for (int x = 0; x < chunkW; x++)
		{
			for (int y = 0; y < chunkH; y++)
			{
				CBaseEntity@ ent = chunks[0][x][y];
				uint c = g_num_to_chunk[Math.RandomLong(0, display_cfg.chunk.numPixelCombinations-1)];
				
				g_EntityFuncs.SetModel(ent, display_cfg.chunk.chunk_path + (c >> 16) + ".mdl");
				ent.pev.skin = (c >> 8) & 0xff;
				ent.pev.body = c & 0xff;
			}
		}
		g_Scheduler.SetTimeout("inc_delay", 0.06666667, this);
	}
}

void MapInit()
{
	g_Hooks.RegisterHook( Hooks::Player::ClientSay, @ClientSay );
	
	for (int i = 0; i < display_cfg.chunk.numChunkModels; i++)
		g_Game.PrecacheModel(display_cfg.chunk.chunk_path + g_quality + i + ".mdl");
		
	for (uint i = 0; i < 64; i++)
	{
		g_SoundSystem.PrecacheSound("display/" + i + ".wav");
		g_Game.PrecacheGeneric("sound/" + "display/" + i + ".wav"); // yay, no more .res file hacking
	}
}

void MapActivate()
{
	init();
}

TraceResult TraceLook(CBasePlayer@ plr, float dist=128)
{
	Vector vecSrc = plr.GetGunPosition();
	Math.MakeVectors( plr.pev.v_angle ); // todo: monster angles
	
	TraceResult tr;
	Vector vecEnd = vecSrc + g_Engine.v_forward * dist;
	g_Utility.TraceLine( vecSrc, vecEnd, dont_ignore_monsters, plr.edict(), tr );
	return tr;
}

void createDisplay(Vector pos, Vector angles)
{
	g_disp.orient(pos, angles);
	g_disp.createChunks();
}

void clearCommInput()
{
	File@ f = g_FileSystem.OpenFile( sound_comms_path2, OpenFile::WRITE);
	if( f !is null && f.IsOpen() )
		f.Remove();
}

void checkNewVideo() 
{
	File@ f = g_FileSystem.OpenFile( sound_comms_path2, OpenFile::READ);
	
	if( f !is null && f.IsOpen() )
	{
		string line;
		f.ReadLine(line);
		array<string> input = line.Split(" ");
		if (input.length() < 2) {
			f.Close();
			return; // not written to yet
		}
		println("GOT NEW CHUNKS MAYBE: " + input[0] + " " + atof(input[1]));
		f.Close();
		clearCommInput();

		g_disp.loadChunks(input[0], atof(input[1]));
		
		return;
	}
	
	g_Scheduler.SetTimeout("checkNewVideo", 0.5);
}

bool lolwut = false;
int loltest = 7*250;

// for some reason topcolor/bottomcolor can't be changed in scripts for anything except players?
void changeModelColor(CBaseEntity@ ent, int color)
{
	dictionary ckeys;
	
	ckeys["target"] = "!caller";
	ckeys["m_iszValueName"] = "bottomcolor";
	ckeys["m_iszNewValue"] = "" + color;
	CBaseEntity@ colorChange = g_EntityFuncs.CreateEntity("trigger_changevalue", ckeys, true);
	colorChange.Use(ent, ent, USE_ON);
	g_EntityFuncs.Remove(colorChange);
	
	ckeys["target"] = "!caller";
	ckeys["m_iszValueName"] = "topcolor";
	ckeys["m_iszNewValue"] = "" + color;
	@colorChange = g_EntityFuncs.CreateEntity("trigger_changevalue", ckeys, true);
	colorChange.Use(ent, ent, USE_ON);
	g_EntityFuncs.Remove(colorChange);
}

void noInterp()
{
	CBaseEntity@ ent = g_EntityFuncs.FindEntityByClassname(ent, "item_generic");
	
	uint c = g_num_to_chunk[loltest];
	loltest += 1;
	
	uint modelIdx = c >> 24;
	uint skinIdx = (c >> 16) & 0xff;
	uint bodyIdx = (c >> 8) & 0xff;
	uint frameIdx = c & 0xff;
	
	println("DISPALY " + modelIdx + " " + skinIdx + " " + bodyIdx + " " + frameIdx);
	
	//g_EntityFuncs.SetModel(ent, display_cfg.chunk.chunk_path + g_quality + modelIdx + ".mdl");
	//ent.pev.skin = skinIdx;
	//ent.pev.body = bodyIdx;
	//ent.pev.frame = frameIdx;
	
	//changeModelColor(ent, 85);
	changeModelColor(ent, 170);
}

bool doDoomCommand(CBasePlayer@ plr, const CCommand@ args)
{	
	bool isAdmin = g_PlayerFuncs.AdminLevel(plr) >= ADMIN_YES;
	
	if ( args.ArgC() > 0 )
	{
		if (args[0] == "c")
		{
			CBaseEntity@ ent = null;
			do {
				@ent = g_EntityFuncs.FindEntityByTargetname(ent, "dchunk");
				if (ent !is null) {
					g_EntityFuncs.Remove(ent);
				}
			} while (ent !is null);
	
			display_cfg = display_cfg_3bit_grey_ld;
			
			g_disp = Display(Vector(0,0,0), Vector(0,0,0), display_cfg.width, display_cfg.height, 4, display_cfg.rgb);	
			init();
			CBaseEntity@ disp_target = g_EntityFuncs.FindEntityByTargetname(null, "display_ori");
			createDisplay(disp_target.pev.origin, disp_target.pev.angles);
			g_PlayerFuncs.SayText(plr, "Changed display\n");
			
			clearCommInput();
			g_disp.loadChunks("chunks.dat", 30.0f);
			
			return true;
		}
		if (args[0] == "y")
		{
			CBaseEntity@ disp_target = g_EntityFuncs.FindEntityByTargetname(null, "display_ori");
			g_PlayerFuncs.SayText(plr, "Create display\n");
			//TraceResult tr = TraceLook(plr, 128);
			//createDisplay(tr.vecEndPos, plr.pev.angles);
			createDisplay(disp_target.pev.origin, disp_target.pev.angles);
			return true;
		}
		if (args[0] == "download" or args[0] == "d")
		{
			// https://github.com/nficano/pytube/pull/313#issuecomment-438175656
			clearCommInput();
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=arp49jiSr1Q');		// biba shooting stars (not work)
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=6JaY3vtb760');		// where's the caveman
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=X6oUz1v17Uo');		// picard song
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=632kyLysP6c');		// Gabe newell e3
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=Ssh71hePR8Q');		// McRolled
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=6n3pFFPSlW4');		// gnomed
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=IwzUs1IMdyQ');		// vitas 7th (not work)
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=ygI-2F8ApUM');		// brody quest
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=A_EeFZUBbog');		// naked snake big boss
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=0tdyU_gW6WE');		// bustin' (not work)
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=h4VuBnRxAaw');		// shaggy's crisis
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=zuDtACzKGRs');		// das boot
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=-Z7UnO66q9w');		// frindship is manly
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=zPiAK_RP8dU');		// sci scream
			g_disp.loadNewVideo('https://www.youtube.com/watch?v=9wnNW4HyDtg');	// ayayaya
			//g_disp.loadNewVideo('https://www.youtube.com/shorts/JkjgK3zuvPI');	// the prophecy is true
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=zZdVwTjUtjg&pp=ygUSb3V0IG9mIHRvdWNoIHJlbWl4');	// the prophecy is true
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=3Fu8ZxBmcnU');	// human bean stutter kid
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=ni9FCzIOX2w');   // human bean flex guy
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=uYmaRNpd-WQ');	// human bean dedotated wam
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=6cAyrdoVpZc');	// human bean office guy
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=kNdDROtrPcQ');		// human bean captcha
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=rbEy8EO8Yko');		// jimmy creepy thing
			g_Scheduler.SetTimeout("checkNewVideo", 0.5);
			return true;
		}
		if (args[0].Find("http") == 0)
		{
			clearCommInput();
			g_disp.loadNewVideo(args[0]);
			g_Scheduler.SetTimeout("checkNewVideo", 0.5);
			return true;
		}
		if (args[0] == "test" or args[0] == "t")
		{
			//g_Scheduler.SetInterval("noInterp", 0.1, -1);
		
			
		
			clearCommInput();
			g_disp.loadChunks("chunks.dat", 30.0f);
			return true;
		}
	}
	return false;
}

HookReturnCode ClientSay( SayParameters@ pParams )
{
	CBasePlayer@ plr = pParams.GetPlayer();
	const CCommand@ args = pParams.GetArguments();	
	if (doDoomCommand(plr, args))
	{
		pParams.ShouldHide = true;
		return HOOK_HANDLED;
	}
	return HOOK_CONTINUE;
}

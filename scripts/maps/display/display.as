
// Limits:
// Sprites - 256 frames max
// Models - 256 submodels max, 100 textures max
// GL_MAXTEXTURES at 4096 textures (~40 models)
// 1x19 can't work with 4x pixel size because of 1024 tex size limit

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
	uint numSkinsLastModel;
	
	int chunkSize;
	int numPixelCombinations;
	int bits; // bits per pixel
	
	string chunk_path;
	
	ChunkModelConfig() {}
	
	ChunkModelConfig(int chunkWidth, int chunkHeight, int bits, int numChunkModels, int skinsPerModel, uint numSkinsLastModel) {
		this.chunkWidth = chunkWidth;
		this.chunkHeight = chunkHeight;
		this.numChunkModels = numChunkModels;
		this.numSkinsLastModel = numSkinsLastModel;
		this.skinsPerModel = skinsPerModel;
		this.bits = bits;
		
		chunkSize = chunkWidth * chunkHeight;
		numPixelCombinations = int( pow(pow(2,bits), chunkSize) );
		
		chunk_path = "models/display/" + bits + "bit/";
	}
}

// 1bit 6x3
ChunkModelConfig chunk_cfg_1bit = ChunkModelConfig(6, 3, 1, 11, 100, 24);
DisplayConfig display_cfg_1bit_grey = DisplayConfig(chunk_cfg_1bit, 126, 72, false);
DisplayConfig display_cfg_1bit_rgb  = DisplayConfig(chunk_cfg_1bit, 72, 42, true);

// 2bit 3x3
ChunkModelConfig chunk_cfg_2bit = ChunkModelConfig(3, 3, 2, 11, 100, 24);
DisplayConfig display_cfg_2bit_grey = DisplayConfig(chunk_cfg_2bit, 87, 51, false);
DisplayConfig display_cfg_2bit_rgb  = DisplayConfig(chunk_cfg_2bit, 51, 30, true);

// 3bit 3x2
ChunkModelConfig chunk_cfg_3bit = ChunkModelConfig(3, 2, 3, 11, 100, 24);
DisplayConfig display_cfg_3bit_grey = DisplayConfig(chunk_cfg_3bit, 72, 42, false);
DisplayConfig display_cfg_3bit_rgb  = DisplayConfig(chunk_cfg_3bit, 42, 24, true);

// 4bit 2x2 (ineffecient)
ChunkModelConfig chunk_cfg_4bit = ChunkModelConfig(2, 2, 4, 3, 100, 56);
DisplayConfig display_cfg_4bit_grey = DisplayConfig(chunk_cfg_4bit, 58, 32, false);
DisplayConfig display_cfg_4bit_rgb  = DisplayConfig(chunk_cfg_4bit, 34, 20, true);

// 6bit 3x1
ChunkModelConfig chunk_cfg_6bit = ChunkModelConfig(3, 1, 6, 11, 100, 24);
DisplayConfig display_cfg_6bit_grey = DisplayConfig(chunk_cfg_6bit, 51, 30, false);
DisplayConfig display_cfg_6bit_rgb  = DisplayConfig(chunk_cfg_6bit, 30, 16, true);

// 8bit 2x1 (ineffecient)
ChunkModelConfig chunk_cfg_8bit = ChunkModelConfig(2, 1, 8, 3, 100, 56);
DisplayConfig display_cfg_8bit_grey = DisplayConfig(chunk_cfg_8bit, 42, 24, false);
DisplayConfig display_cfg_8bit_rgb  = DisplayConfig(chunk_cfg_8bit, 24, 14, true);


//
// ~~~~~~~~~~~~~~~~~~~ CHOOSE CONFIG HERE ~~~~~~~~~~~~~~~~~~~
//
DisplayConfig display_cfg = display_cfg_4bit_grey;
//
// ~~~~~~~~~~~~~~~~~~~ CHOOSE CONFIG HERE ~~~~~~~~~~~~~~~~~~~
//


Display g_disp = Display(Vector(0,0,0), Vector(0,0,0), display_cfg.width, display_cfg.height, 2, display_cfg.rgb);	

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

void init()
{
	int mapSize = int( pow(2, 4*4) );
	
	uint body = 0;
	uint skin = 0;
	uint model = 0;
	g_num_to_chunk.resize(display_cfg.chunk.numPixelCombinations);
	for (int i = 0; i < display_cfg.chunk.numPixelCombinations; i++)
	{
		g_num_to_chunk[i] = (model << 16) + (skin << 8) + body;
		body += 1;
		if (body >= 256)
		{
			body = 0;
			skin += 1;
			if (int(skin) >= display_cfg.chunk.skinsPerModel or (int(model) == display_cfg.chunk.numChunkModels-1 and skin >= display_cfg.chunk.numSkinsLastModel))
			{
				skin = 0;
				model += 1;
				if (int(model) >= display_cfg.chunk.numChunkModels)
					model = 0;
			}
		}
	}
}

string sound_comms_path = "scripts/maps/temp/sound_comms";
string sound_comms_path2 = "scripts/maps/temp/sound_comms2";

class Display
{
	int width, height;
	int chunkW, chunkH, chans;
	float scale;
	float fps = 30.0f;
	Vector pos, angles;
	bool rgb_mode;
	array<string> rgb_dirs = {"red/", "green/", "blue/"};
	
	Vector up, right, forward;
	
	array<array<array<EHandle>>> chunks;
	
	File@ f = null;
	int frameCounter = 0;
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
		
			
		println("Created " + chunkW + "x" + chunkH + " display (" + (chunkW*chunkH) + " ents)");
		
		orient(pos, angles);
	}
	
	void orient(Vector pos, Vector angles)
	{
		g_EngineFuncs.MakeVectors(angles);
		up = g_Engine.v_up;
		right = g_Engine.v_right;
		forward = g_Engine.v_forward;
		
		this.pos = pos + up*(chunkH/2)*display_cfg.chunk.chunkHeight*scale + -right*(chunkW/2)*display_cfg.chunk.chunkWidth*scale;
		this.angles = angles;
	}
	
	void loadNewVideo(string url)
	{
		@sound_comms = g_FileSystem.OpenFile( sound_comms_path, OpenFile::WRITE);
		sound_comms.Write("load " + url + " " + display_cfg.bits + " " + display_cfg.rgb);
		sound_comms.Close();
	}
	
	void createChunks()
	{
		int channels = display_cfg.rgb ? 3 : 1;
		
		for (int c = 0; c < channels; c++)
		{
			string color_dir = rgb_mode ? rgb_dirs[c] : "grey/";
			
			dictionary ckeys;
			ckeys["model"] = display_cfg.chunk.chunk_path + color_dir + "0.mdl";
			ckeys["movetype"] = "5";
			ckeys["scale"] = "" + scale;
			ckeys["targetname"] = "display_sprite";
			ckeys["angles"] = (angles + Vector(0,180,0)).ToString();
			ckeys["rendermode"] = "5";
			ckeys["renderamt"] = "1";
			
			for (int x = 0; x < chunkW; x++)
			{
				for (int y = 0; y < chunkH; y++)
				{
					Vector chunkPos = pos + right*x*display_cfg.chunk.chunkWidth*scale + -up*y*display_cfg.chunk.chunkHeight*scale;
					
					//chunkPos = chunkPos - forward*c*scale*2;
					//chunkPos = chunkPos + right*x*1 + up*y*-1;
					
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
				return;
			}
			
			// start the sound
			@sound_comms = g_FileSystem.OpenFile( sound_comms_path, OpenFile::WRITE);
			sound_comms.Write("play");
			sound_comms.Close();
			
			videoStartTime = g_Engine.time;
			
			frameCounter = 0;
		}
		
		float targetFrame = (g_Engine.time - videoStartTime) * fps;
		//println("" + frameCounter + " " + targetFrame);			
		
		string line;
		while( !f.EOFReached() and frameCounter < targetFrame )
		{
			f.ReadLine(line);		
			
			array<string> chunkValues = line.Split(" ");
			int chunkTotal = chunkW*chunkH;
			
			if (chunkTotal*chans != int(chunkValues.size()))
			{
				println("Frame data does not match screen size " + chunkValues.size() + " " + (chunkW*chunkH));
				@f = null;
				g_Scheduler.SetTimeout("inc_delay", 0.05, this);
				return;
			}
			
			for (int ch = 0; ch < chans; ch++)
			{
				string color_dir = rgb_mode ? rgb_dirs[ch] : "grey/";
				for (int y = 0; y < chunkH; y++)
				{
					for (int x = 0; x < chunkW; x++)
					{
						CBaseEntity@ ent = chunks[ch][x][y];
						uint value = atoi(chunkValues[chunkTotal*ch + y*chunkW + x]);
						uint c = g_num_to_chunk[value];
						uint modelIdx = c >> 16;
						uint skinIdx = (c >> 8) & 0xff;
						uint bodyIdx = c & 0xff;
						
						g_EntityFuncs.SetModel(ent, display_cfg.chunk.chunk_path + color_dir + modelIdx + ".mdl");
						ent.pev.skin = skinIdx;
						ent.pev.body = bodyIdx;
					}
				}
			}
			if (++frameCounter >= int(targetFrame))
				break;
		}
		
		
		//println("Loaded frame " + frameCounter);
		//g_Scheduler.SetTimeout("inc_delay", 0.06666667, this);
		g_Scheduler.SetTimeout("inc_delay", 0.01, this);
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
	{
		if (display_cfg.rgb)
		{
			g_Game.PrecacheModel(display_cfg.chunk.chunk_path + "red/" + i + ".mdl");
			g_Game.PrecacheModel(display_cfg.chunk.chunk_path + "green/" + i + ".mdl");
			g_Game.PrecacheModel(display_cfg.chunk.chunk_path + "blue/" + i + ".mdl");
		}
		else
			g_Game.PrecacheModel(display_cfg.chunk.chunk_path + "grey/" + i + ".mdl");
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
		println("GOT NEW CHUNKS MAYBE: " + input[0] + " " + atof(input[1]));
		f.Close();
		clearCommInput();

		g_disp.loadChunks(input[0], atof(input[1]));
		
		return;
	}
	
	g_Scheduler.SetTimeout("checkNewVideo", 0.5);
}

bool doDoomCommand(CBasePlayer@ plr, const CCommand@ args)
{	
	bool isAdmin = g_PlayerFuncs.AdminLevel(plr) >= ADMIN_YES;
	
	if ( args.ArgC() > 0 )
	{
		if (args[0] == "y")
		{
			g_PlayerFuncs.SayText(plr, "Create display\n");
			TraceResult tr = TraceLook(plr, 128);
			createDisplay(tr.vecEndPos, plr.pev.angles);
			return true;
		}
		if (args[0] == "download" or args[0] == "d")
		{
			// https://github.com/nficano/pytube/pull/313#issuecomment-438175656
			clearCommInput();
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=zPiAK_RP8dU');
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=9wnNW4HyDtg');	// ayayaya
			g_disp.loadNewVideo('https://www.youtube.com/watch?v=3Fu8ZxBmcnU');		// human bean stutter kid
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=ni9FCzIOX2w');   // human bean flex guy
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=uYmaRNpd-WQ');	// human bean dedotated wam
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=6cAyrdoVpZc');
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=kNdDROtrPcQ');
			//g_disp.loadNewVideo('https://www.youtube.com/watch?v=rbEy8EO8Yko');		// jimmy creepy thing
			g_Scheduler.SetTimeout("checkNewVideo", 0.5);
			return true;
		}
		if (args[0] == "test" or args[0] == "t")
		{
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

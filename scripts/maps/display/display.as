
// Limits:
// Sprites - 256 frames max
// Models - 256 submodels max, 100 textures max
// GL_MAXTEXTURES at 4096 textures (~40 models)
// 1x19 can't work with 4x pixel size because of 1024 tex size limit

// 1bit 6x3
/*
int numChunkModels = 11;
int chunkSize = 18;
int chunkWidth = 6;
int chunkHeight = 3;
uint numSkinsLastModel = 64;
int skinsPerModel = 96;
int numPixelCombinations = int( pow(2, chunkSize) );
//string chunk_fname = "chunks_1bit.dat";
string display_path = "models/display/1bit/";
*/

// 2bit 3x3
/*
int numChunkModels = 11;
int chunkSize = 9;
int chunkWidth = 3;
int chunkHeight = 3;
uint numSkinsLastModel = 24;
int skinsPerModel = 100;
int numPixelCombinations = int( pow(4, chunkSize) );
//string chunk_fname = "chunks_2bit.dat";
string display_path = "models/display/2bit/";
*/

// 3bit 3x2
int numChunkModels = 11;
int chunkSize = 6;
int chunkWidth = 3;
int chunkHeight = 2;
uint numSkinsLastModel = 24;
int skinsPerModel = 100;
int numPixelCombinations = int( pow(8, chunkSize) );
//string chunk_fname = "chunks_8bit.dat";
string display_path = "models/display/3bit/";

string chunk_fname = "chunks_3bit_rgb.dat";
bool g_rgb_mode = true;


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
	disp.loadFrame();
}

array<uint> g_num_to_chunk; // converts binary number to chunk details (model + body + skin)

void init()
{
	int mapSize = int( pow(2, 4*4) );
	
	uint body = 0;
	uint skin = 0;
	uint model = 0;
	g_num_to_chunk.resize(numPixelCombinations);
	for (int i = 0; i < numPixelCombinations; i++)
	{
		g_num_to_chunk[i] = (model << 16) + (skin << 8) + body;
		body += 1;
		if (body >= 256)
		{
			body = 0;
			skin += 1;
			if (int(skin) >= skinsPerModel or (int(model) == numChunkModels-1 and skin >= numSkinsLastModel))
			{
				skin = 0;
				model += 1;
				if (int(model) >= numChunkModels)
					model = 0;
			}
		}
	}
}

class Display
{
	int width, height;
	int chunkW, chunkH, chans;
	float scale;
	Vector pos, angles;
	bool rgb_mode;
	array<string> rgb_dirs = {"red/", "green/", "blue/"};
	
	Vector up, right;
	
	array<array<array<EHandle>>> chunks;
	
	File@ f = null;
	int frameCounter = 0;
	
	Display() {}
	
	Display(Vector pos, Vector angles, uint width, uint height, float scale, bool rgb_mode)
	{
		if (width % chunkWidth != 0 or height % chunkHeight != 0)
			println("Display size rounded to nearest multiple of " + chunkWidth + "x" + chunkHeight);
		this.width = width - (width % chunkWidth);
		this.height = height - (height % chunkHeight);
		this.chunkW = this.width / chunkWidth;
		this.chunkH = this.height / chunkHeight;
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
		
		g_EngineFuncs.MakeVectors(angles);
		up = g_Engine.v_up;
		right = g_Engine.v_right;
		
		this.pos = pos + up*(chunkH/2)*chunkHeight*scale + -right*(chunkW/2)*chunkWidth*scale;
		this.angles = angles;
		
		createChunks();
	}
	
	void createChunks()
	{
		int channels = g_rgb_mode ? 3 : 1;
		
		
		for (int c = 0; c < channels; c++)
		{
			string color_dir = rgb_mode ? rgb_dirs[c] : "grey/";
			
			dictionary ckeys;
			ckeys["model"] = display_path + color_dir + "0.mdl";
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
					Vector chunkPos = pos + right*x*chunkWidth*scale + -up*y*chunkHeight*scale;
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
	
	void loadFrame()
	{
		if (f is null) {
			println("LOAD AGAIN");
			string fpath = "scripts/maps/display/" + chunk_fname;
			@f = g_FileSystem.OpenFile( fpath, OpenFile::READ );
			if( f is null or !f.IsOpen())
			{
				println("Failed to open " + fpath);
				return;
			}
			frameCounter = 0;
		}

		string line;
		while( !f.EOFReached() )
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
						
						g_EntityFuncs.SetModel(ent, display_path + color_dir + modelIdx + ".mdl");
						ent.pev.skin = skinIdx;
						ent.pev.body = bodyIdx;
					}
				}
			}
			break;
		}
		
		frameCounter++;
		println("Loaded frame " + frameCounter);
		//g_Scheduler.SetTimeout("inc_delay", 0.06666667, this);
		g_Scheduler.SetTimeout("inc_delay", 0.0333, this);
	}
	
	void inc()
	{
		for (int x = 0; x < chunkW; x++)
		{
			for (int y = 0; y < chunkH; y++)
			{
				CBaseEntity@ ent = chunks[0][x][y];
				uint c = g_num_to_chunk[Math.RandomLong(0, numPixelCombinations-1)];
				
				g_EntityFuncs.SetModel(ent, display_path + (c >> 16) + ".mdl");
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
	
	for (int i = 0; i < numChunkModels; i++)
	{
		if (g_rgb_mode)
		{
			g_Game.PrecacheModel(display_path + "red/" + i + ".mdl");
			g_Game.PrecacheModel(display_path + "green/" + i + ".mdl");
			g_Game.PrecacheModel(display_path + "blue/" + i + ".mdl");
		}
		else
			g_Game.PrecacheModel(display_path + i + ".mdl");
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
	//Display disp = Display(pos, angles, 126, 72, 1, g_rgb_mode); // 1bit
	//Display disp = Display(pos, angles, 72, 42, 1, g_rgb_mode); // 1bit RGB
	//Display disp = Display(pos, angles, 87, 51, 1.5, g_rgb_mode); // 2bit
	//Display disp = Display(pos, angles, 51, 30, 2.5, g_rgb_mode); // 2bit RGB
	//Display disp = Display(pos, angles, 72, 42, 2, g_rgb_mode); // 4bit
	Display disp = Display(pos, angles, 42, 24, 2.5, g_rgb_mode); // 4bit RGB
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

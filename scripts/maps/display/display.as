
// Limits:
// Sprites - 256 frames max
// Models - 256 submodels max, 100 textures max
// GL_MAXTEXTURES at 4096 textures (~40 models)
// 1x19 can't work with 4x pixel size because of 1024 tex size limit

// 2bit 6x3
/*
int numChunkModels = 11;
int chunkSize = 18;
int chunkWidth = 6;
int chunkHeight = 3;
uint numSkinsLastModel = 64;
int skinsPerModel = 96;
int numPixelCombinations = int( pow(2, chunkSize) );
*/

// 4bit 3x3
/*
int numChunkModels = 11;
int chunkSize = 9;
int chunkWidth = 3;
int chunkHeight = 3;
uint numSkinsLastModel = 24;
int skinsPerModel = 100;
int numPixelCombinations = int( pow(4, chunkSize) );
*/

// 8bit 3x2
int numChunkModels = 11;
int chunkSize = 6;
int chunkWidth = 3;
int chunkHeight = 2;
uint numSkinsLastModel = 24;
int skinsPerModel = 100;
int numPixelCombinations = int( pow(8, chunkSize) );

// 1x19
/*
int numChunkModels = 21;
int chunkSize = 19;
int chunkWidth = 19;
int chunkHeight = 1;
*/


void print(string text) { g_Game.AlertMessage( at_console, text); }
void println(string text) { print(text + "\n"); }


string display_path = "models/display/";

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
	int chunkW, chunkH;
	float scale;
	Vector pos, angles;
	
	Vector up, right;
	
	array<array<EHandle>> chunks;
	
	File@ f = null;
	int frameCounter = 0;
	
	Display() {}
	
	Display(Vector pos, Vector angles, uint width, uint height, float scale)
	{
		if (width % chunkWidth != 0 or height % chunkHeight != 0)
			println("Display size rounded to nearest multiple of " + chunkWidth + "x" + chunkHeight);
		this.width = width - (width % chunkWidth);
		this.height = height - (height % chunkHeight);
		this.chunkW = this.width / chunkWidth;
		this.chunkH = this.height / chunkHeight;
		this.scale = scale;
		
		chunks.resize(this.chunkW);
		for (int x = 0; x < chunkW; x++)
			chunks[x].resize(this.chunkH);
			
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
		dictionary ckeys;
		ckeys["model"] = display_path + "0.mdl";
		ckeys["movetype"] = "5";
		ckeys["scale"] = "" + scale;
		ckeys["targetname"] = "display_sprite";
		ckeys["angles"] = (angles + Vector(0,180,0)).ToString();
		
		for (int x = 0; x < chunkW; x++)
		{
			for (int y = 0; y < chunkH; y++)
			{
				Vector chunkPos = pos + right*x*chunkWidth*scale + -up*y*chunkHeight*scale;
				ckeys["origin"] = chunkPos.ToString();
				CBaseEntity@ spr = g_EntityFuncs.CreateEntity("item_generic", ckeys, true);
				spr.pev.solid = SOLID_NOT;
				//spr.pev.rendermode = 5;
				
				chunks[x][y] = EHandle(spr);
			}
		}
		
		g_Scheduler.SetTimeout("inc_delay", 0.05, this);
	}
	
	void loadFrame()
	{
		if (f is null) {
			println("LOAD AGAIN");
			string fpath = "scripts/maps/display/chunks.dat";
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
			
			if (chunkW*chunkH != int(chunkValues.size()))
			{
				println("Frame data does not match screen size " + chunkValues.size() + " " + (chunkW*chunkH));
				@f = null;
				g_Scheduler.SetTimeout("inc_delay", 0.05, this);
				return;
			}
			
			for (int y = 0; y < chunkH; y++)
			{
				for (int x = 0; x < chunkW; x++)
				{
					CBaseEntity@ ent = chunks[x][y];
					uint value = atoi(chunkValues[y*chunkW + x]);
					uint c = g_num_to_chunk[value];
					uint modelIdx = c >> 16;
					uint skinIdx = (c >> 8) & 0xff;
					uint bodyIdx = c & 0xff;
					
					g_EntityFuncs.SetModel(ent, display_path + modelIdx + ".mdl");
					ent.pev.skin = skinIdx;
					ent.pev.body = bodyIdx;
				}
			}
			break;
		}
		
		frameCounter++;
		println("Loaded frame " + frameCounter);
		g_Scheduler.SetTimeout("inc_delay", 0.06666667, this);
	}
	
	void inc()
	{
		for (int x = 0; x < chunkW; x++)
		{
			for (int y = 0; y < chunkH; y++)
			{
				CBaseEntity@ ent = chunks[x][y];
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
		g_Game.PrecacheModel(display_path + i + ".mdl");
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
	//Display disp = Display(pos, angles, 126, 72, 1); // 2bit max
	//Display disp = Display(pos, angles, 87, 51, 1.5); // 4bit max
	Display disp = Display(pos, angles, 72, 42, 2); // 8bit max
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

#include "main.h"
#include "mmlib.h"
#include <cstdio>
#include "subprocess.h"
#include "lodepng.h"
#include "Display.h"
#include "AudioPlayer.h"
#include "crc32.h"
#include "VideoPlayer.h"

#pragma comment(linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8")
#pragma comment(linker, "/SECTION:.data,RW")

using namespace std;

// Description of plugin
plugin_info_t Plugin_info = {
	META_INTERFACE_VERSION,	// ifvers
	"MediaPlayer",	// name
	"1.0",	// version
	__DATE__,	// date
	"w00tguy",	// author
	"https://github.com/wootguy/",	// url
	"MEDIA",	// logtag, all caps please
	PT_ANYTIME,	// (when) loadable
	PT_ANYPAUSE,	// (when) unloadable
};

VideoPlayer* g_video_player = NULL;

float lastCommand = 0;
EHandle g_host;
bool g_enabled = true;

bool commandCooldown(edict_t* plr) {
	if (g_host.IsValid() && ENTINDEX(g_host.GetEdict()) != ENTINDEX(plr)) {
		ClientPrint(plr, HUD_PRINTTALK, UTIL_VarArgs("[Video] %s must stop hosting before you can use this command.\n", STRING(g_host.GetEdict()->v.netname)));
		return true;
	}
	if (g_engfuncs.pfnTime() - lastCommand < 2.0f) {
		ClientPrint(plr, HUD_PRINTTALK, "[Video] Wait a second before using another command.\n");
		return true;
	}

	lastCommand = g_engfuncs.pfnTime();
	return false;
}

bool doCommand(edict_t* plr) {
	CommandArgs args = CommandArgs();
	args.loadArgs();
	string lowerArg = toLowerCase(args.ArgV(0));

	if (lowerArg == ".host") {
		if (g_host.IsValid()) {
			if (ENTINDEX(g_host.GetEdict()) == ENTINDEX(plr)) {
				ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] %s quit hosting.\n", STRING(plr->v.netname)));
				g_host = EHandle();
				return true;
			}
			else {
				ClientPrint(plr, HUD_PRINTTALK, UTIL_VarArgs("[Video] %s must stop hosting first.\n", STRING(g_host.GetEdict()->v.netname)));
				return true;
			}
		}
		else {
			g_host = plr;
			ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] %s is now the host.\n", STRING(plr->v.netname)));
		}
	}
	if (lowerArg == ".stop") {
		if (commandCooldown(plr)) { return true; }
		g_video_player->stopVideo();
		g_video_player->videoQueue.clear();
		ClientPrintAll(HUD_PRINTTALK, "[Video] Stopped video and cleared queue.\n");
	}
	if (lowerArg == ".skip") {
		if (commandCooldown(plr)) { return true; }
		g_video_player->skipVideo();
	}
/*
	if (lowerArg == ".pause") {
		if (commandCooldown(plr)) { return true; }
		g_video_player->pause();
	}
*/
	if (lowerArg == ".dlight") {
		if (commandCooldown(plr)) { return true; }
		g_video_player->m_disp.dlight = !g_video_player->m_disp.dlight;
		if (g_video_player->m_disp.dlight)
			ClientPrintAll(HUD_PRINTTALK, "[Video] Dynamic light enabled.\n");
		else
			ClientPrintAll(HUD_PRINTTALK, "[Video] Dynamic light disabled.\n");
	}
	if (lowerArg == ".reliable") {
		uint32_t plrBit = 1 << (ENTINDEX(plr) & 31);
		bool wasReliable = g_video_player->audio_player->reliableMode & plrBit;

		if (wasReliable) {
			ClientPrint(plr, HUD_PRINTTALK, "[Video] Reliable audio packets disabled.\n");
			g_video_player->audio_player->reliableMode &= ~plrBit;
		}
		else {
			ClientPrint(plr, HUD_PRINTTALK, "[Video] Reliable audio packets enabled.\n");
			g_video_player->audio_player->reliableMode |= plrBit;
		}

		return true;
	}
	if (lowerArg == ".queue") {
		if (commandCooldown(plr)) { return true; }

		if (g_video_player->videoQueue.size() == 0) {
			ClientPrint(plr, HUD_PRINTTALK, "[Video] The queue is empty.\n");
			return true;
		}

		ClientPrint(plr, HUD_PRINTTALK, "[Video] queue written to your console.\n");
		ClientPrint(plr, HUD_PRINTCONSOLE, "---- Video queue\n");
		for (int i = 0; i < g_video_player->videoQueue.size(); i++) {
			string title = g_video_player->videoQueue[i].keyavlues["title"];
			ClientPrint(plr, HUD_PRINTCONSOLE, UTIL_VarArgs("%d) %s\n", i+1, title.c_str()));
		}
		ClientPrint(plr, HUD_PRINTCONSOLE, "----\n");
		return true;
	}
	if (lowerArg == ".mode") {
		string arg = toLowerCase(args.ArgV(1));
		if (arg.size() != 2 || !(arg[1] == 'g' || arg[1] == 'c') || !isdigit(arg[0])) {
			ClientPrint(plr, HUD_PRINTTALK, "Usage: .mode [bits (1-8)][c/g]\n");
			ClientPrint(plr, HUD_PRINTTALK, "   Ex: \".mode 4c\" = 4-bit color\n");
			ClientPrint(plr, HUD_PRINTTALK, "   Ex: \".mode 7g\" = 7-bit greyscale\n");
			return true;
		}
		bool rgb = arg[1] == 'c';
		int bits = atoi(arg.substr(0).c_str());

		if (bits < 1 || bits > 8) {
			ClientPrint(plr, HUD_PRINTTALK, "Invalid bits. Choose 1-8.\n");
			return true;
		}

		if (commandCooldown(plr)) { return true; }

		//ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] Set display mode to %d-bit %s\n", bits, rgb ? "color" : "greyscale"));

		g_video_player->setMode(bits, rgb, g_video_player->wantFps);
	}
	if (lowerArg == ".fps") {
		string arg = toLowerCase(args.ArgV(1));
		if (arg.size() == 0) {
			ClientPrint(plr, HUD_PRINTTALK, "Usage: .fps [1-30]\n");
			return true;
		}
		int fps = clamp(atoi(arg.substr(0).c_str()), 1, 30);

		if (commandCooldown(plr)) { return true; }
		ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] Set display FPS to %d\n", fps));

		g_video_player->setMode(0, 0, fps);
		g_video_player->restartVideo();
	}
	if (lowerArg == ".sync") {
		string arg = toLowerCase(args.ArgV(1));
		if (arg.size() == 0) {
			ClientPrint(plr, HUD_PRINTTALK, "Usage: .sync [+/-1000]\n");
			return true;
		}
		int sync = clamp(atoi(arg.substr(0).c_str()), -1000, 1000);

		if (commandCooldown(plr)) { return true; }
		ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] Audio sync set to %dms\n", sync));

		g_video_player->syncDelay = (float)sync * 0.001f;
	}
	if (lowerArg == ".radio") {
		ClientPrintAll(HUD_PRINTTALK, "[Video] The radio plugin is disabled on this map.\n");
	}
	if (lowerArg == ".replay") {
		if (commandCooldown(plr)) { return true; }
		g_video_player->restartVideo();
	}
	if (lowerArg == ".demo") {
		//g_video_player->play("https://www.youtube.com/watch?v=zZdVwTjUtjg");
		//g_video_player->play("https://www.youtube.com/shorts/JkjgK3zuvPI");

		if (commandCooldown(plr)) { return true; }
		ClientPrintAll(HUD_PRINTTALK, "[Video] Playing demo video\n");

		g_video_player->setMode(2, false, 30);
		g_video_player->play("https://www.youtube.com/watch?v=FtutLA63Cp8");
	}

	if (args.ArgV(0).find("http://") == 0 || args.ArgV(0).find("https://") == 0)
	{
		if (commandCooldown(plr)) { return false; }
		g_video_player->play(args.ArgV(0));
		return false;
	}

	return false;
}

bool initDone = false;
bool radioPaused = false;

void StartFrame() {
	if (!g_enabled) {
		RETURN_META(MRES_IGNORED);
	}

    g_Scheduler.Think();
	handleThreadPrints();

	if (!initDone && gpGlobals->time > 2.0f) {
		initDone = true;
		for (int i = 0; i < 8; i++) {
			g_chunk_configs[i].init();
		}
	}
	if (!radioPaused && gpGlobals->time > 2.0f) {
		radioPaused = true;

		// prevent conflicts with chat urls and audio
		// it will unpause itself on map change
		g_engfuncs.pfnServerCommand("meta pause radio\n");
		g_engfuncs.pfnServerExecute();
	}

	if (g_host.IsValid() && (g_host.GetEdict()->v.deadflag != 0 || !isValidPlayer(g_host))) {
		ClientPrintAll(HUD_PRINTTALK, "[Video] The host is gone.\n");
		g_host = EHandle();
	}

	g_video_player->think();

    RETURN_META(MRES_IGNORED);
}

void ClientJoin(edict_t* plr) {
	uint32_t plrBit = 1 << (ENTINDEX(plr) & 31);
	g_video_player->audio_player->reliableMode &= ~plrBit;

	RETURN_META(MRES_IGNORED);
}

void ClientCommand(edict_t* pEntity) {
	if (!g_enabled) {
		RETURN_META(MRES_IGNORED);
	}

	META_RES ret = doCommand(pEntity) ? MRES_SUPERCEDE : MRES_IGNORED;
	RETURN_META(ret);
}

void MapInit(edict_t* pEdictList, int edictCount, int maxClients) {
	radioPaused = false;

	g_enabled = string(STRING(gpGlobals->mapname)).find("cinema") == 0;
	if (!g_enabled) {
		RETURN_META(MRES_IGNORED);
	}

	PrecacheModel("models/display/1bit/ld/0.mdl");
	PrecacheModel("models/display/2bit/ld/0.mdl");
	PrecacheModel("models/display/3bit/ld/0.mdl");
	PrecacheModel("models/display/4bit/ld/0.mdl");
	PrecacheModel("models/display/5bit/ld/0.mdl");
	PrecacheModel("models/display/6bit/ld/0.mdl");
	PrecacheModel("models/display/7bit/ld/0.mdl");
	PrecacheModel("models/display/8bit/ld/0.mdl");
	PrecacheModel("sprites/cinema/loading.spr");

	RETURN_META(MRES_IGNORED);
}

void MapChange() {
	g_video_player->unload();
	RETURN_META(MRES_IGNORED);
}

void PluginInit() {
	g_dll_hooks.pfnStartFrame = StartFrame;
	g_dll_hooks.pfnClientCommand = ClientCommand;
	g_dll_hooks.pfnServerActivate = MapInit;
	g_dll_hooks.pfnServerDeactivate = MapChange;
	g_dll_hooks.pfnClientPutInServer = ClientJoin;

	g_main_thread_id = std::this_thread::get_id();

	crc32_init();

	g_video_player = new VideoPlayer();

	if (gpGlobals->time > 2.0f)
		g_enabled = string(STRING(gpGlobals->mapname)).find("cinema") == 0;
}

void PluginExit() {
	delete g_video_player;
}
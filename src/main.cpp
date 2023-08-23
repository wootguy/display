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

bool doCommand(edict_t* plr) {
	CommandArgs args = CommandArgs();
	args.loadArgs();
	string lowerArg = toLowerCase(args.ArgV(0));

    if (lowerArg == "t") {
		println("TEST META PLAYER");

		g_video_player->stopVideo();
		g_video_player->playNewVideo(0);

		/*
		const char* command = "ffmpeg -i input.mp4 -f rawvideo -pix_fmt rgb24 - 2>/dev/null";

		FILE* pipe = popen(command, "r");
		if (!pipe) {
			std::cerr << "Failed to open pipe for command: " << command << std::endl;
			return 1;
		}
		*/

        return true;
    }
	if (lowerArg == "s") {
		g_video_player->stopVideo();
	}
	if (lowerArg == "d") {
		g_video_player->play("https://www.youtube.com/watch?v=zZdVwTjUtjg", 30);
	}

	if (args.ArgV(0).find("http") == 0)
	{
		g_video_player->play(args.ArgV(0), 30);
		return false;
	}

	return false;
}



void StartFrame() {
    g_Scheduler.Think();
	handleThreadPrints();

	g_video_player->think();

    RETURN_META(MRES_IGNORED);
}

void ClientCommand(edict_t* pEntity) {
	META_RES ret = doCommand(pEntity) ? MRES_SUPERCEDE : MRES_IGNORED;
	RETURN_META(ret);
}

void MapInit(edict_t* pEdictList, int edictCount, int maxClients) {
	for (int i = 0; i < display_cfg.chunk.numChunkModels; i++)
		PrecacheModel((display_cfg.chunk.chunk_path + g_quality + to_string(i) + ".mdl").c_str());

	RETURN_META(MRES_IGNORED);
}

void MapChange() {
	g_video_player->stopVideo();
	RETURN_META(MRES_IGNORED);
}

void PluginInit() {
	g_dll_hooks.pfnStartFrame = StartFrame;
	g_dll_hooks.pfnClientCommand = ClientCommand;
	g_dll_hooks.pfnServerActivate = MapInit;
	g_dll_hooks.pfnServerDeactivate = MapChange;

	g_main_thread_id = std::this_thread::get_id();

	crc32_init();

	g_video_player = new VideoPlayer();
}

void PluginExit() {
	delete g_video_player;
}
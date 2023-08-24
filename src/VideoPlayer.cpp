#include "VideoPlayer.h"
#include <string>
#include <vector>
#include "AudioPlayer.h"
#include "Display.h"
#include "subprocess.h"

using namespace std;

#define MAX_DISPLAY_PIXELS (120*66) // no video can be larger than this

const char* video_buffer_file = "svencoop_addon/scripts/maps/display/video.mkv";
const char* audio_buffer_file = "svencoop_addon/scripts/maps/display/audio.mp3";
const char* python_script_path = "svencoop_addon/scripts/maps/display/test.py";

#ifdef WIN32
const char* python_program = "python";
#else
const char* python_program = "python3";
#endif

long filesize(const char* filename)
{
	FILE* fp = fopen(filename, "r");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long fileSize = ftell(fp);
		fclose(fp);
		return fileSize;
	}
	return 0;
}

VideoPlayer::VideoPlayer() {
	audio_player = new AudioPlayer(1);

	frameData = new color24[MAX_DISPLAY_PIXELS * 3];
	memset(frameData, 0, MAX_DISPLAY_PIXELS * 3 * sizeof(color24));
}

VideoPlayer::~VideoPlayer() {
	if (frameData) {
		delete[] frameData;
	}
	killAllChildren();
	m_disp.destroy();

	REMOVE_ENTITY(loadingSpr);

	for (int i = 0; i < MAX_PLAYERS; i++) {
		audio_player->exitSignal = true;
	}

	println("Waiting for audio player thread to join...");

	delete audio_player;
}

void VideoPlayer::init() {
	m_disp = Display(&g_chunk_configs[4], false);

	edict_t* disp_target = FIND_ENTITY_BY_TARGETNAME(NULL, "display_ori");
	m_disp.createChunks();
	m_disp.orient(disp_target->v.origin, disp_target->v.angles, m_disp.scale);

	edict_t* ent = NULL;
	do {
		ent = FIND_ENTITY_BY_TARGETNAME(ent, "cinema_audio");
		if (isValidFindEnt(ent)) {
			audio_areas.push_back(ent);
		}
	} while (isValidFindEnt(ent));

	println("Found %d areas", audio_areas.size());

	map<string, string> ckeys;
	ckeys["model"] = "sprites/cinema/loading.spr";
	ckeys["scale"] = "0.5";
	ckeys["angles"] = "0 0 0";
	ckeys["origin"] = vecToString(m_disp.pos + m_disp.forward*-4);
	ckeys["vp_type"] = "4";
	ckeys["renderamt"] = "0";
	ckeys["rendermode"] = "1";
	ckeys["framerate"] = "10";
	ckeys["spawnflags"] = "1";

	loadingSpr = CreateEntity("env_sprite", ckeys, true);
}

void VideoPlayer::play(string url) {
	if (videoQueue.size() >= MAX_QUEUE) {
		ClientPrintAll(HUD_PRINTTALK, "[Video] The queue is full!");
		return;
	}

	canFastReplay = false;
	loadVideoInfo(url);
	lastUrl = url;
}

void VideoPlayer::pause() {
	paused = !paused;
}

void VideoPlayer::skipVideo() {
	if (videoQueue.size() == 0) {
		ClientPrintAll(HUD_PRINTTALK, "[Video] Video stopped. The queue is empty.");
		stopVideo();
		return;
	}
	else {
		ClientPrintAll(HUD_PRINTTALK, "[Video] Video skipped.");
		stopVideo();
		loadNextQueueVideo();
	}
}

void VideoPlayer::loadNextQueueVideo() {
	Video vid = videoQueue[0];
	videoQueue.erase(videoQueue.begin());

	m_python_output = vid.python_output;
	loadNewVideo();
	ClientPrintAll(HUD_PRINTNOTIFY, "[Video] Loading next video.\n");
}

void VideoPlayer::setMode(int bits, bool rgb, float fps) {
	bool wasPlaying = m_video_playing || m_video_downloading || m_video_buffering;
	this->wantFps = fps;

	stopVideo();
	if (bits > 0)
		m_disp.setMode(bits, rgb);
	canFastReplay = false;

	if (wasPlaying) {
		restartVideo();
	}
}

void VideoPlayer::restartVideo() {
	if (canFastReplay) {
		ClientPrintAll(HUD_PRINTTALK, "[Video] Replaying last video\n");
		m_disp.clear();
		audio_player->stop();
		m_video_playing = false;
		m_video_buffering = false;
		frameIdx = 0;

		killChildProcess(decodePid);

		playNewVideo(0);
	}
	else {
		ClientPrintAll(HUD_PRINTNOTIFY, "[Video] Restreaming last video\n");
		play(lastUrl);
	}
}

void VideoPlayer::convertFrame() {
	int w = m_disp.width;
	int h = m_disp.height;
	int channels = m_disp.chans;
	int bits = m_disp.cfg->bits;

	int chunkSizeX = m_disp.cfg->chunkWidth;
	int chunkSizeY = m_disp.cfg->chunkHeight;

	int cw = w / chunkSizeX;
	int ch = h / chunkSizeY;
	int chunksPerFrame = cw * ch * channels;

	// RGB + lightness dominance in this frame(is this image mostly blue / red, etc.)
	float dominance[4] = { 0, 0, 0, 0 };
	float maxBright = 255 * w * h * 3;
	uint32_t bdominance[4];
	for (int py = 0; py < h; py++) {
		for (int px = 0; px < w; px++) {
			int offset = py * actualWidth + px;

			byte r = frameData[offset].r;
			byte g = frameData[offset].g;
			byte b = frameData[offset].b;
			dominance[0] += r;
			dominance[1] += g;
			dominance[2] += b;
			dominance[3] += (r * 0.35 + g * 0.40 + b * 0.25); // blue light is less bright than green / red
		}
	}
	for (int i = 0; i < 4; i++) {
		bdominance[i] = 255 * (dominance[i] / maxBright);
	}

	vector<uint32_t> chunks;
	for (int chan = 0; chan < channels; chan++) {
		for (int cy = 0; cy < ch; cy++) {
			for (int cx = 0; cx < cw; cx++) {
				uint32_t chunkValue = 0;
				uint32_t pixelValue = 1; // determines which bit to set in the chunk
				uint32_t numShifts = 0;

				for (int y = 0; y < chunkSizeY; y++) {
					for (int x = 0; x < chunkSizeX; x++) {
						int px = cx * chunkSizeX + x;
						int py = cy * chunkSizeY + y;

						if (px >= actualWidth || py >= actualHeight) {
							continue;
						}
						
						if (channels == 1) {
							// greyscale
							color24 color = frameData[py * actualWidth + px];
							// some colors are brighter than others
							byte val = (byte)(color.r*0.35f + color.g*0.40f + color.b*0.25f) >> (8 - bits);
							chunkValue |= m_disp.cfg->chunkBits[val] << numShifts;
						}
						else {
							int offset = (py * actualWidth + px) * sizeof(color24);
							byte val = ((byte*)frameData)[offset + chan] >> (8 - bits);
							chunkValue |= m_disp.cfg->chunkBits[val] << numShifts;
						}
						

						pixelValue <<= 1;
						numShifts += 1;
					}
				}

				m_disp.setChunkValue(cx, cy, chan, chunkValue);
			}
		}
	}

	if (channels == 1) {
		byte grey = (byte)(bdominance[0] * 0.35f + bdominance[1] * 0.40f + bdominance[2] * 0.25f);
		m_disp.setLightValue(grey, grey, grey, grey);
	}
	else {
		m_disp.setLightValue(bdominance[0], bdominance[1], bdominance[2], bdominance[3]);
	}
	
}

void VideoPlayer::readFfmpegOutput(int subpid) {
	int bytesRead = 0;

	float audioDelay = 0.15f; // time to slow down video to keep in sync with audio, which is buffered on the client
	int desiredFrame = (audio_player->getPlaybackTime() - audioDelay) * actualFps;

	if (desiredFrame < frameIdx) {
		//println("wait for audio to catch up");
		return;
	}

	int wantBytes = actualWidth * actualHeight * sizeof(color24);

	readChildProcessStdout(subpid, (char*)frameData, wantBytes, bytesRead);

	if (bytesRead > 0 || isProcessAlive(subpid)) {
		if (wantBytes == bytesRead) {
			convertFrame();
		}

		frameIdx++;
	}
	else {
		decodePid = 0;

		if (!m_video_downloading) {
			println("Video finished playing at %d frames", frameIdx);
			
			m_disp.clear();
			m_video_playing = false;

			if (videoQueue.size()) {
				loadNextQueueVideo();
			}
			else {
				ClientPrintAll(HUD_PRINTNOTIFY, "[Video] finished.\n");
			}
		}
		else {
			m_video_buffering = true;
			nextVideoPlay = g_engfuncs.pfnTime() + 2;
			nextPlayOffset = 0;
			ClientPrintAll(HUD_PRINTNOTIFY, "[Video] buffering...\n");
			println("Video buffering at frame %d...", frameIdx);
		}
	}
}

void VideoPlayer::monitorVideoDownloadProcess(int subpid) {
	static char buffer[512];
	int bytesRead = 0;

	readChildProcessStdout(subpid, (char*)buffer, 512, bytesRead);

	if (bytesRead > 0 || isProcessAlive(subpid)) {
		if (bytesRead > 0) {
			//printp("%s", output.c_str());
		}
	}
	else {
		float time = g_engfuncs.pfnTime() - downloadStartTime;
		ClientPrintAll(HUD_PRINTNOTIFY, UTIL_VarArgs("[Video] Finished downloading in %ds (%.1fx rate)\n", (int)time, duration / time));
		m_video_downloading = false;
		downloadPid = 0;
		int videoSize = filesize(video_buffer_file);
		int audioSize = filesize(audio_buffer_file);
		if (videoSize == 0 || audioSize == 0) {
			ClientPrintAll(HUD_PRINTTALK, "[Video] Failed to load:\n");
			ClientPrintAll(HUD_PRINTTALK, "  Video/audio stream missing.\n");
		}
	}

	if (!m_video_playing && !m_video_buffering) {
		int videoSize = filesize(video_buffer_file);
		int audioSize = filesize(audio_buffer_file);
		if (videoSize > 1024 && audioSize > 1024) {
			println("Start with sizes %dk %dk", videoSize / 1024, audioSize / 1024);
			ClientPrintAll(HUD_PRINTNOTIFY, "[Video] Playback starting\n");
			m_video_buffering = true;
			m_video_playing = true;
			nextVideoPlay = g_engfuncs.pfnTime() + 2;
			nextPlayOffset = 0;
		}
	}
}


void VideoPlayer::sizeToFit(int& width, int& height) {
	int chunkW = m_disp.cfg->chunkWidth;
	int chunkH = m_disp.cfg->chunkHeight;
	
	int bestWidth = chunkW;
	int bestHeight = chunkH;

	float ratio = (float)height / width;

	println("Content size: %dx%d (%.2f aspect)", width, height, ratio);

	while (true) {
		int testWidth = bestWidth + chunkW;
		int testHeight = (((int)(testWidth * ratio) / chunkH) * chunkH);

		int numChunks = (testWidth / chunkW) * (testHeight / chunkH) * m_disp.chans;

		if (numChunks > MAX_CHUNKS) {
			break;
		}

		bestWidth = testWidth;
		bestHeight = testHeight;
	}

	int numChunks = (bestWidth / chunkW) * (bestHeight / chunkH) * m_disp.chans;
	println("Best dims: %dx%d (%d/%d chunks)", bestWidth, bestHeight, numChunks, MAX_CHUNKS);

	string fpsString = UTIL_VarArgs("%d fps", actualFps);
	if (actualFps < wantFps) {
		fpsString += UTIL_VarArgs(" (%d unavailable)", wantFps);
	}
	ClientPrintAll(HUD_PRINTNOTIFY, UTIL_VarArgs("[Video] %d-bit %s, %dx%d pixels, %s\n", 
		m_disp.cfg->bits, m_disp.rgb ? "color" : "greyscale", bestWidth, bestHeight, fpsString.c_str()));
	
	// resize display to fit this content
	m_disp.resize(bestWidth, bestHeight);

	// content should be resized to this (ffmpeg required multiple of 2 dimensions)
	width = ((bestWidth + 1) / 2) * 2;
	height = ((bestHeight + 1) / 2) * 2;
}

bool VideoPlayer::parsePythonOutput(string python_str, map<string, string>& outputMap) {
	string header = "---MEDIA_PLAYER_MM_INFO_BEGINS---";
	int outstart = python_str.find(header);
	if (outstart == -1) {
		string errorHeader = "---MEDIA_PLAYER_MM_ERROR_BEGINS---";
		int errstart = python_str.find(errorHeader);

		ClientPrintAll(HUD_PRINTTALK, "[Video] Failed to load:\n");
		if (errstart != -1) {
			string err = "  " + trimSpaces(python_str.substr(errstart + errorHeader.size())) + "\n";
			ClientPrintAll(HUD_PRINTTALK, err.substr(0, 255).c_str());
		}
		else {
			ClientPrintAll(HUD_PRINTTALK, "(no reason given)\n");
		}

		return false;
	}

	string output = trimSpaces(python_str.substr(outstart + header.size()));

	vector<string> settings = splitString(output, "\n");

	for (int i = 0; i < settings.size(); i++) {
		int firsteq = settings[i].find_first_of("=");
		if (firsteq == -1) {
			continue;
		}
		string name = trimSpaces(settings[i].substr(0, firsteq));
		string value = trimSpaces(settings[i].substr(firsteq + 1));

		outputMap[name] = value;

		//println("%s = %s", name.c_str(), value.c_str());
	}

	return true;
}

void VideoPlayer::loadNewVideo() {
	stopVideo();
	
	remove(video_buffer_file);
	remove(audio_buffer_file);

	map<string, string> keyvals;
	if (!parsePythonOutput(m_python_output, keyvals)) {
		return;
	}

	//println("URL IS %s", keyvals["url"].c_str());

	ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] %s\n", keyvals["title"].c_str()));

	int offset = 0;
	int width = atoi(keyvals["width"].c_str());
	int height = atoi(keyvals["height"].c_str());
	actualFps = Min(wantFps, atoi(keyvals["fps"].c_str()));
	duration = atof(keyvals["length"].c_str());

	sizeToFit(width, height);

	actualWidth = width;
	actualHeight = height;

	string video_codec = UTIL_VarArgs("-c:v libx264 -preset veryfast -crf 18 -filter:v fps=%d -s %dx%d -tune fastdecode -map 0:v:0 %s",
		actualFps, width, height, video_buffer_file);

	
	string loudnorm_filter = "-af loudnorm=I=-22:LRA=11:TP=-1.5";
	string audio_codec = "-c:a libmp3lame -q:a 6 -ar 12000 " + loudnorm_filter + " -map 0:a:0 " + audio_buffer_file;

	#ifdef WIN32
	keyvals["url"] = "\"" + keyvals["url"] + "\"";
	#else
	keyvals["url"] = "'" + keyvals["url"] + "'";
	#endif
	
	string ffmpeg_cmd = "ffmpeg -hide_banner -loglevel error -threads 1 -y -i " + keyvals["url"] + " " + video_codec + " " + audio_codec;

	if (ffmpeg_cmd.size() > 512) {
		printp("%s", ffmpeg_cmd.substr(0, 512).c_str());
		println("%s", ffmpeg_cmd.substr(512).c_str());
	} else {
		println("%s", ffmpeg_cmd.c_str());
	}
	
	int subpid = createChildProcess(ffmpeg_cmd.c_str());

	if (subpid != 0) {
		downloadStartTime = g_engfuncs.pfnTime();
		m_video_downloading = true;
		downloadPid = subpid;
		canFastReplay = true;

		if (loadingSpr.IsValid()) {
			loadingSpr.GetEdict()->v.renderamt = 255;
		}
	}
	else {
		println("Failed to create child process");
	}
}

void VideoPlayer::readPythonOutput(int subpid) {
	static char buffer[512];

	int bytesRead = 0;

	readChildProcessStdout(subpid, (char*)buffer, 512, bytesRead);

	if (bytesRead > 0 || isProcessAlive(subpid)) {
		if (bytesRead > 0) {
			string output = string(buffer, bytesRead);
			m_python_output += output;
			printp("%s", output.c_str());
		}
	}
	else {
		if (m_video_playing || m_video_buffering) {
			map<string, string> keyvals;
			if (!parsePythonOutput(m_python_output, keyvals)) {
				return;
			}

			Video vid;
			vid.python_output = m_python_output;
			vid.keyavlues = keyvals;
			videoQueue.push_back(vid);
			ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] Queued (%d/%d): %s", videoQueue.size(), MAX_QUEUE, keyvals["title"].c_str()));
		}
		else {
			loadNewVideo();
		}
		
		m_python_output = "";
		m_python_running = false;
		pythonPid = 0;
	}
}

void VideoPlayer::loadVideoInfo(string url) {
	m_python_output = "";
	m_python_running = true;

	const char* cmd = UTIL_VarArgs("%s %s %s", python_program, python_script_path, url.c_str());
	println(cmd);
	int subpid = createChildProcess(cmd);

	if (subpid != 0) {
		pythonPid = subpid;
	}
	else {
		println("Failed to create child process");
	}
}

void VideoPlayer::playNewVideo(int frameOffset) {
	if (m_video_playing && !m_video_buffering) {
		println("Wait for video to stop!");
		return;
	}

	audio_player->playMp3(audio_buffer_file);

	//int subpid = createChildProcess("ffmpeg -version");
	float seekto = float(frameOffset) / actualFps;
	const char* cmd = UTIL_VarArgs("ffmpeg -threads 1 -hide_banner -loglevel error -i %s -ss %.2f -f rawvideo -pix_fmt rgb24 -", video_buffer_file, seekto);
	println("%s", cmd);
	int subpid = createChildProcess(cmd);

	if (subpid != 0) {
		m_video_buffering = false;
		m_video_playing = true;
		decodePid = subpid;
		frameIdx = frameOffset;
		readFfmpegOutput(subpid);

		if (loadingSpr.IsValid()) {
			loadingSpr.GetEdict()->v.renderamt = 0;
		}
	}
	else {
		println("Failed to create child process");
	}
}

void VideoPlayer::stopVideo() {
	println("Stopping video");
	killAllChildren();
	if (displayCreated)
		m_disp.clear();
	audio_player->stop();
	m_video_playing = false;
	m_video_buffering = false;
	m_video_downloading = false;
	paused = false;
	
	nextVideoPlay = 0;
	pythonPid = 0;
	decodePid = 0;
	downloadPid = 0;
	frameIdx = 0;

	if (loadingSpr.IsValid()) {
		loadingSpr.GetEdict()->v.renderamt = 0;
	}
}

void VideoPlayer::updateSpeakerIdx() {
	int speakerIdx = 1;
	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		edict_t* plr = INDEXENT(i);
		if (!isValidPlayer(plr)) {
			speakerIdx = i;
			break;
		}
	}
	audio_player->playerIdx = speakerIdx;
}

void VideoPlayer::unload() {
	displayCreated = false;
	videoQueue.clear();
	stopVideo();
}

void VideoPlayer::think() {
	if (!displayCreated) {
		if (gpGlobals->time > 2.0f) {
			init();
			displayCreated = true;
		}
		return;
	}

	if (pythonPid != 0) {
		readPythonOutput(pythonPid);
	}
	if (downloadPid != 0) {
		monitorVideoDownloadProcess(downloadPid);
	}
	if (decodePid != 0) {
		if (!paused)
			readFfmpegOutput(decodePid);
	}
	if (nextVideoPlay && nextVideoPlay < g_engfuncs.pfnTime()) {
		nextVideoPlay = 0;
		playNewVideo(0);
	}
	updateSpeakerIdx();	

	if (!paused)
		audio_player->play_samples();

	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		edict_t* plr = INDEXENT(i);
		
		if (!isValidPlayer(plr)) {
			continue;
		}

		bool inAudibleArea = false;
		Vector ori = plr->v.origin;
		for (int k = 0; k < audio_areas.size(); k++) {
			edict_t* area = audio_areas[k];
			if (!area) {
				continue;
			}
			
			Vector mins = area->v.absmin;
			Vector maxs = area->v.absmax;
			if (ori.x > mins.x && ori.x < maxs.x 
				&& ori.y > mins.y && ori.y < maxs.y
				&& ori.z > mins.z && ori.z < maxs.z) {
				inAudibleArea = true;
				break;
			}
		}

		uint32_t plrBit = 1 << (i & 31);
		if (inAudibleArea) {
			audio_player->listeners |= plrBit;
		}
		else {
			audio_player->listeners &= ~plrBit;
		}
	}

	string err;
	if (audio_player->errors.dequeue(err)) {
		ClientPrintAll(HUD_PRINTNOTIFY, err.c_str());
	}
}

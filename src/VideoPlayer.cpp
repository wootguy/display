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

	for (int i = 0; i < MAX_PLAYERS; i++) {
		audio_player->exitSignal = true;
	}

	println("Waiting for audio player thread to join...");

	delete audio_player;
}

void VideoPlayer::init() {
	m_disp = Display(Vector(0, 0, 0), Vector(0, 0, 0), display_cfg.width, display_cfg.height, 4, display_cfg.rgb);

	edict_t* disp_target = FIND_ENTITY_BY_TARGETNAME(NULL, "display_ori");
	m_disp.orient(disp_target->v.origin, disp_target->v.angles);
	m_disp.createChunks();
}

void VideoPlayer::play(string url, float fps) {
	loadVideoInfo(url);
}

void VideoPlayer::convertFrame() {
	int w = m_disp.width;
	int h = m_disp.height;
	int channels = m_disp.chans;
	int bits = display_cfg.bits;

	int chunkSizeX = display_cfg.chunk.chunkWidth;
	int chunkSizeY = display_cfg.chunk.chunkHeight;

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

						if (bits == 3) {
							int offset = (py * actualWidth + px) * sizeof(color24);

							byte val = ((byte*)frameData)[offset + chan];

							float step = 36.43;
							int totalSteps = int((255.0 / step) + 0.5);
							bool invalid = true;
							int bestStep = 0;
							float bestDiff = 999;

							for (int k = 0; k < totalSteps; k++) {
								float diff = abs((float)val - step * (k + 1));
								if (diff < bestDiff) {
									bestStep = k;
									bestDiff = diff;
								}
							}

							chunkValue |= m_disp.chunkBits[bestStep + 1] << numShifts;
						}

						pixelValue <<= 1;
						numShifts += 1;
					}
				}

				m_disp.setChunkValue(cx, cy, chan, chunkValue);
			}
		}
	}

	m_disp.setLightValue(bdominance[0], bdominance[1], bdominance[2], bdominance[3]);
}

void VideoPlayer::readFfmpegOutput(int subpid) {
	int bytesRead = 0;

	float audioDelay = 0.15f; // time to slow down video to keep in sync with audio, which is buffered on the client
	int desiredFrame = (audio_player->getPlaybackTime() - audioDelay) * m_disp.fps;

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
			ClientPrintAll(HUD_PRINTNOTIFY, "Video finished.\n");
			m_disp.clear();
			m_video_playing = false;
		}
		else {
			m_video_buffering = true;
			nextVideoPlay = g_engfuncs.pfnTime() + 2;
			nextPlayOffset = frameIdx;
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

void VideoPlayer::loadNewVideo() {
	stopVideo();
	
	remove(video_buffer_file);
	remove(audio_buffer_file);

	string header = "---MEDIA_PLAYER_MM_INFO_BEGINS---";
	int outstart = m_python_output.find(header);
	if (outstart == -1) {
		string errorHeader = "---MEDIA_PLAYER_MM_ERROR_BEGINS---";
		int errstart = m_python_output.find(errorHeader);

		ClientPrintAll(HUD_PRINTTALK, "[Video] Failed to load:\n");
		if (errstart != -1) {
			string err = "  " + trimSpaces(m_python_output.substr(errstart + errorHeader.size())) + "\n";
			ClientPrintAll(HUD_PRINTTALK, err.substr(0,255).c_str());
		}
		else {
			ClientPrintAll(HUD_PRINTTALK, "(no reason given)\n");
		}

		return;
	}

	string output = trimSpaces(m_python_output.substr(outstart + header.size()));

	vector<string> settings = splitString(output, "\n");
	map<string, string> keyvals;

	for (int i = 0; i < settings.size(); i++) {
		int firsteq = settings[i].find_first_of("=");
		if (firsteq == -1) {
			continue;
		}
		string name = trimSpaces(settings[i].substr(0, firsteq));
		string value = trimSpaces(settings[i].substr(firsteq + 1));

		keyvals[name] = value;

		//println("%s = %s", name.c_str(), value.c_str());
	}

	//println("URL IS %s", keyvals["url"].c_str());

	ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[Video] %s\n", keyvals["title"].c_str()));

	int offset = 0;
	int fps = 15;
	int width = atoi(keyvals["width"].c_str());
	int height = atoi(keyvals["height"].c_str());
	duration = atof(keyvals["length"].c_str());

	float ratio = (float)width / height;
	float inv_ratio = (float)height / width;

	int maxWidth = display_cfg.width;
	int maxHeight = display_cfg.height;

	if (width > maxWidth) {
		width = maxWidth;
		height = ((int)(width * inv_ratio) / 2) * 2;
	}

	if (height > maxHeight) {
		height = maxHeight;
		width = ((int)(height * ratio) / 2) * 2;
	}

	actualWidth = width;
	actualHeight = height;

	string video_codec = UTIL_VarArgs("-c:v libx264 -preset veryfast -crf 18 -filter:v fps=%d -s %dx%d -tune fastdecode -map 0:v:0 %s",
		fps, width, height, video_buffer_file);

	
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
		loadNewVideo();
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
	float seekto = float(frameOffset) / m_disp.fps;
	const char* cmd = UTIL_VarArgs("ffmpeg -threads 1 -hide_banner -loglevel error -i %s -ss %.2f -f rawvideo -pix_fmt rgb24 -", video_buffer_file, seekto);
	println("%s", cmd);
	int subpid = createChildProcess(cmd);

	if (subpid != 0) {
		m_video_buffering = false;
		m_video_playing = true;
		decodePid = subpid;
		frameIdx = frameOffset;
		readFfmpegOutput(subpid);
	}
	else {
		println("Failed to create child process");
	}
}

void VideoPlayer::stopVideo() {
	println("Stopping video");
	killAllChildren();
	m_disp.clear();
	audio_player->stop();
	m_video_playing = false;
	m_video_buffering = false;
	m_video_downloading = false;

	pythonPid = 0;
	decodePid = 0;
	downloadPid = 0;
	frameIdx = 0;
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
		readFfmpegOutput(decodePid);
	}
	if (nextVideoPlay && nextVideoPlay < g_engfuncs.pfnTime()) {
		nextVideoPlay = 0;
		playNewVideo(0);
	}
	updateSpeakerIdx();

	audio_player->play_samples();

	string err;
	if (audio_player->errors.dequeue(err)) {
		ClientPrintAll(HUD_PRINTNOTIFY, err.c_str());
	}
}

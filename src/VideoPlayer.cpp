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
	int channels = 3;
	int bits = 3;

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
								float diff = abs(val - step * (k + 1));
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

	if (peekChildProcessStdout(subpid, (char*)frameData, wantBytes) >= wantBytes) {
		readChildProcessStdout(subpid, (char*)frameData, wantBytes, bytesRead);
	}

	if (bytesRead > 0 || isProcessAlive(subpid)) {
		if (wantBytes == bytesRead) {
			convertFrame();
		}

		frameIdx++;
	}
	else {
		decodePid = -1;

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

	if (peekChildProcessStdout(subpid, (char*)buffer, 512) > 0) {
		readChildProcessStdout(subpid, (char*)buffer, 512, bytesRead);
	}

	if (bytesRead > 0 || isProcessAlive(subpid)) {
		if (bytesRead > 0) {
			//printp("%s", output.c_str());
		}

		if (!m_video_playing && !m_video_buffering) {
			if (filesize(video_buffer_file) > 0) {
				println("File has been written to! Begin playback soon");
				m_video_buffering = true;
				m_video_playing = true;
				nextVideoPlay = g_engfuncs.pfnTime() + 3;
				nextPlayOffset = 0;
			}
		}
	}
	else {
		println("Video finished downloading");
		m_video_downloading = false;
		downloadPid = -1;
	}
}

void VideoPlayer::loadNewVideo() {
	stopVideo();

	string header = "---MEDIA_PLAYER_MM_INFO_BEGINS---";
	int outstart = m_python_output.find("---MEDIA_PLAYER_MM_INFO_BEGINS---");
	if (outstart == -1) {
		println("Failed to find video info");
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

	ClientPrintAll(HUD_PRINTNOTIFY, UTIL_VarArgs("Loading video: %s\n", keyvals["title"].c_str()));

	int offset = 0;
	int fps = 15;
	int width = atoi(keyvals["width"].c_str());
	int height = atoi(keyvals["height"].c_str());

	actualWidth = width;
	actualHeight = height;

	string video_codec = UTIL_VarArgs("-c:v libx264 -preset veryfast -crf 18 -filter:v fps=%d -s %dx%d -tune fastdecode -map 0:v:0 %s",
		fps, width, height, video_buffer_file);

	const char* audio_output_path = "svencoop_addon/scripts/maps/display/audio.mp3";
	string audio_codec = UTIL_VarArgs("-c:a libmp3lame -q:a 6 -ar 12000 -map 0:a:0 %s", audio_output_path);

	const char* ffmpem_cmd = UTIL_VarArgs("ffmpeg -hide_banner -threads 1 -y -i %s %s %s",
		keyvals["url"].c_str(), video_codec.c_str(), audio_codec.c_str());

	int subpid = createChildProcess(ffmpem_cmd);

	if (subpid != -1) {
		m_video_downloading = true;
		downloadPid = subpid;
		monitorVideoDownloadProcess(subpid);
	}
	else {
		println("Failed to create child process");
	}
}

void VideoPlayer::readPythonOutput(int subpid) {
	static char buffer[512];

	int bytesRead = 0;

	if (peekChildProcessStdout(subpid, (char*)buffer, 512) > 0) {
		readChildProcessStdout(subpid, (char*)buffer, 512, bytesRead);
	}

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
		pythonPid = -1;
	}
}

void VideoPlayer::loadVideoInfo(string url) {
	m_python_output = "";
	m_python_running = true;

	const char* script_path = "svencoop_addon/scripts/maps/display/test.py";
	int subpid = createChildProcess(UTIL_VarArgs("python %s %s", script_path, url.c_str()));

	if (subpid != -1) {
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

	if (!audio_player->playMp3(audio_buffer_file)) {
		println("Failed to load audio file");
		return;
	}

	//int subpid = createChildProcess("ffmpeg -version");
	float seekto = float(frameOffset) / m_disp.fps;
	const char* cmd = UTIL_VarArgs("ffmpeg -threads 1 -hide_banner -loglevel error -i %s -ss %.2f -f rawvideo -pix_fmt rgb24 -", video_buffer_file, seekto);
	println(cmd);
	int subpid = createChildProcess(cmd);

	if (subpid != -1) {
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
	killAllChildren();
	m_disp.clear();
	audio_player->stop();
	m_video_playing = false;
	m_video_buffering = false;
	m_video_downloading = false;

	pythonPid = -1;
	decodePid = -1;
	downloadPid = -1;
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

	if (pythonPid != -1) {
		readPythonOutput(pythonPid);
	}
	if (downloadPid != -1) {
		monitorVideoDownloadProcess(downloadPid);
	}
	if (decodePid != -1) {
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
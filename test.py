import traceback, json, os, sys, time, subprocess
import yt_dlp as youtube_dl

def load_info_from_url(url):
	with youtube_dl.YoutubeDL() as ydl:
		ydl_info = ydl.extract_info(url, download=False)
		
		#print(json.dumps(ydl_info, sort_keys=True, indent=4))
		
		title = ydl_info['title']
		length = ydl_info['duration'] if 'duration' in ydl_info else -1
		formats = ydl_info['formats'] if 'formats' in ydl_info else ydl_info['entries'][0]['formats']
		
		streams = [i for i in formats if i.get('acodec', 'none') != 'none' and i.get('vcodec', 'none') != 'none']
		
		best_stream = None
		if len(streams):
			lowest_rez = 99999999
			for stream in streams:
				if stream['width'] is None:
					continue
				#print(json.dumps(stream, sort_keys=True, indent=4))
				rez = stream['width']*stream['height']
				if rez < lowest_rez and stream['fps'] >= 15:
					lowest_rez = rez
					best_stream = stream
		elif len(formats):
			best_stream = formats[0]
		
		if type(title) == list:
			title = title[-1]
		
		print(json.dumps(best_stream, sort_keys=True, indent=4))
		return {'title': title, 'length': length, 'url': best_stream['url'], 'width': best_stream['width'], 'height': best_stream['height']}

def download_video(url, maxWidth, maxHeight, offset):
	try:		
		vid_info = load_info_from_url(url)
			
		playurl = vid_info['url']
		title = vid_info['title']
		length = vid_info['length']
		
		safe_url = playurl.replace(" ", "%20").replace("'", '%27')
		
		width = vid_info['width']
		height = vid_info['height']
		ratio = width / height
		
		if width > maxWidth:
			width = maxWidth
			height = int(int(width* (1.0 / ratio)) / 2) * 2
			
		if height > maxHeight:
			height = maxHeight
			width = int(int(height * ratio) / 2) * 2
		
		fps = 15
		video_codec = '-c:v libx264 -preset veryfast -crf 18 -filter:v fps=%d -s %dx%d -tune fastdecode -map 0:v:0 test.mkv' % (fps, width, height)
		#video_codec = '-c:v rawvideo -pix_fmt rgb24 -filter:v fps=%d -s %dx%d -map 0:v:0 test.avi' % (fps, width, height)
		audio_codec = '-c:a libmp3lame -q:a 6 -ar 12000 -map 0:a:0 test.mp3'
		
		# ffmpeg -y -i test3.mp4 -c:v libx264 -preset veryfast -crf 23 -filter:v fps=15 -s 120x66 -threads 1 -movflags faststart -c:a libmp3lame -q:a 6 test2.mp4
		# ffmpeg -y -i test3.mp4 -threads 1 -c:v libx264 -preset veryfast -crf 23 -filter:v fps=15 -s 120x66 -movflags faststart -map 0:v:0 test2.mkv -c:a libmp3lame -q:a 6 -map 0:a:0 test2.mp3
		# ffmpeg -y -i test3.mp4 -threads 1 -pix_fmt rgb24 -filter:v fps=15 -s 120x66 -map 0:v:0 test2.rgb -c:a libmp3lame -q:a 6 -map 0:a:0 test2.mp3
		
		loudnorm_filter = '-af loudnorm=I=-22:LRA=11:TP=-1.5' # uses too much memory
		cmd = 'ffmpeg -hide_banner -threads 1 -y -ss %s -i %s %s %s' % (offset, safe_url, video_codec, audio_codec)
		ffmpeg = subprocess.Popen(cmd.split(' '))
		ffmpeg.communicate()

	except Exception as e:
		traceback.print_exc()

# max size for any video
maxWidth = 120
maxHeight = 72

# 3 bit RGB
maxWidth = 42
maxHeight = 26

if (len(sys.argv) == 1):
	download_video('https://www.youtube.com/watch?v=zZdVwTjUtjg', maxWidth, maxHeight, 0)
	sys.exit()

vid_info = load_info_from_url(sys.argv[1])

print("---MEDIA_PLAYER_MM_INFO_BEGINS---")

width = vid_info['width']
height = vid_info['height']

ratio = width / height
inv_ratio = height / width
	
if width > maxWidth:
	width = maxWidth
	height = int(int(width* inv_ratio) / 2) * 2
	
if height > maxHeight:
	height = maxHeight
	width = int(int(height * ratio) / 2) * 2

print("width=%s" % width)
print("height=%s" % height)
print("length=%s" % vid_info['length'])
print("title=%s" % vid_info['title'])
print("url=%s" % vid_info['url'])
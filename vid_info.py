import traceback, json, sys
import yt_dlp as youtube_dl

def load_info_from_url(url):
	with youtube_dl.YoutubeDL() as ydl:
		ydl_info = ydl.extract_info(url, download=False)
		
		#print(json.dumps(ydl_info, sort_keys=True, indent=4))
		
		title = ydl_info['title']
		length = ydl_info['duration'] if 'duration' in ydl_info else -1
		formats = [ydl_info]
		if 'formats' in ydl_info:
			formats = ydl_info['formats']
		elif 'entries' in ydl_info:
			formats = ydl_info['entries'][0]['formats']
		
		streams = [i for i in formats if i.get('acodec', 'none') != 'none' and i.get('vcodec', 'none') != 'none']
		
		#print(streams)
		
		best_stream = None
		if len(streams):
			best_fps = 1
			for stream in streams:
				if 'fps' in stream and stream['fps'] >= best_fps:
					best_fps = stream['fps']
			best_fps = min(best_fps, 30)
		
			lowest_rez = 99999999
			for stream in streams:
				if stream['width'] is None:
					continue
				#print(json.dumps(stream, sort_keys=True, indent=4))
				rez = stream['width']*stream['height']
				if rez < lowest_rez and ('fps' not in stream or stream['fps'] >= best_fps):
					lowest_rez = rez
					best_stream = stream
		elif len(formats):
			best_stream = formats[0]
		
		if type(title) == list:
			title = title[-1]
			
		if best_stream is None:
			best_stream = streams[0]
			
		if 'width' not in best_stream or 'height' not in best_stream:
			# just assume this
			best_stream['width'] = 160
			best_stream['height'] = 90
		
		fps = best_stream['fps'] if 'fps' in best_stream else 30
		print(json.dumps(best_stream, sort_keys=True, indent=4))
		return {'title': title, 'length': length, 'url': best_stream['url'], 'width': best_stream['width'], 'height': best_stream['height'], 'fps': fps}

try:
	vid_info = load_info_from_url(sys.argv[1])
except Exception as e:
	print(traceback.format_exc())
	
	print("---MEDIA_PLAYER_MM_ERROR_BEGINS---")
	err_string = str(e).strip().replace('\n','. ').strip()
	print(err_string)
	sys.exit()

print("---MEDIA_PLAYER_MM_INFO_BEGINS---")

print("width=%s" % vid_info['width'])
print("height=%s" % vid_info['height'])
print("length=%s" % vid_info['length'])
print("fps=%s" % vid_info['fps'])
print("title=%s" % vid_info['title'].encode('utf-8', errors='ignore').decode("ascii", "ignore"))
print("url=%s" % vid_info['url'])
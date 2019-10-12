# pip install pygame pyglet pytube pillow
# install avbin + ffmpeg + graphics magick

import pygame
import pyglet
from pytube import YouTube
import os
from os import system
from PIL import Image
import sys, traceback
from threading import Thread
from time import sleep
from queue import Queue
import time
from pprint import pprint
import mmap
from pyglet.media import Player
import subprocess			
import re, io
from threading import Lock

# work shared between threads
convert_queue = Queue()
converted_chunks = []
converting_video = False
comms_path = "../../../../svencoop/scripts/maps/temp/sound_comms"
comms_path2 = "../../../../svencoop/scripts/maps/temp/sound_comms2"
cwd = os.getcwd()
player = Player()
audio_source = None

playerLock = Lock()

def download_video(yt):
	stream = yt.streams.filter(adaptive=True, subtype='mp4', res='144p').first()
	if not stream:
		bestRez = 999999
		for s in yt.streams.filter(adaptive=True, subtype='mp4').all():
			if s.resolution:
				size = int(s.resolution[:-1])
				if size < bestRez:
					bestRez = size
					stream = s
			print("Possible RES: %s" % s.resolution)
	print("Downloading video stream: %s" % stream)
	video = stream.download(output_path='temp', filename='video')
	print("Finished video download")

def download_audio(yt):
	stream = yt.streams.filter(only_audio=True, audio_codec='vorbis').order_by('abr').asc().first()
	if not stream:
		stream = yt.streams.filter(only_audio=True).order_by('abr').asc().first()
	print("Downloading audio stream: %s" % stream)
	audio = stream.download(output_path='temp', filename='audio')
	print("Finished audio download")
	
	print("Converting audio stream")
	if os.path.exists('temp/audio.ogg'):
		os.remove('temp/audio.ogg')
	system("ffmpeg -i %s temp/audio.ogg" % audio)
			
def convert_video(url, bits, greyscale, frameWidth, frameHeight, chunkWidth, chunkHeight):
	global convert_queue
	global converted_chunks
	global converting_video
	global comms_path2
	global cwd
	global player
	global audio_source
	global playerLock
	
	print("Converting video to %s %s-bit %sx%s (%sx%s chunks)" % 
		("Greyscale" if greyscale else "RGB", bits, frameWidth, frameHeight, chunkWidth, chunkHeight))
	
	try:
		print("Querying " + url)
		yt = YouTube(url)
		#print("Video: %s (%s)" % (yt.title, yt.length))
		
		print("Video streams:")
		pprint(yt.streams.filter(adaptive=True, subtype='mp4', res='144p').all())
		print("Audio streams:")
		pprint(yt.streams.filter(only_audio=True).order_by('abr').asc().all())
		
		if True:
			video_thread = Thread(target = download_video, args = (yt, ))
			audio_thread = Thread(target = download_audio, args = (yt, ))
			video_thread.start()
			audio_thread.start()
			video_thread.join()
			audio_thread.join()
		'''
		print("Downloading video stream: %s" % yt.streams.filter(adaptive=True, subtype='mp4', res='144p').first())
		video = yt.streams.filter(adaptive=True, subtype='mp4', res='144p').first().download('temp', filename='video')
		print("Downloading audio stream: %s" % yt.streams.filter(only_audio=True).order_by('abr').asc().first())
		audio = yt.streams.filter(only_audio=True).order_by('abr').asc().first().download(filename='audio')
		'''
		
		fname = 'temp/video.mp4'
		print("Extracting frames from %s" % fname)
		output = subprocess.check_output(['ffmpeg', '-i', 'temp/video.mp4', '-map', '0:v:0', '-c', 'copy', '-f', 'null', '-'], stderr=subprocess.STDOUT)
		output = output.decode('ascii', errors='ignore')
		frameCount = int( re.search("frame=\s*\d+", output, 0).group().split()[1] )
		videoTime = re.search("time=\s*\d\d\:\d\d\:\d\d\.\d\d", output, 0).group().split('=')[1]
		videoTime = videoTime.split(':')
		videoTime = int( (float(videoTime[0])*60*60 + float(videoTime[1])*60 + float(videoTime[2])) )
		framerate = float(frameCount) / float(videoTime)
		framerate = min(framerate, 15)
		frameStep = 1.0 / framerate
		frameCount = int(videoTime*framerate) # adjust for new framerate
		print("ZOMG GOT FRAMES: %s" % frameCount)
		print("ZOMB GOT DURATION: %s" % videoTime)
		print("GOT FRAMERATE: %s" % framerate)
		print("GOT 20fps frames: %s" % int(videoTime * 24.00414937759336))
		
		#system("magick convert " + fname + " temp/%d.jpg")
		
		print("Converting frames")
		
		frameNumbers = []
		for idx in range(frameCount):
			frameNumbers.append(idx)
		converted_chunks = [''] * len(frameNumbers)
		
		convert_queue = Queue()
		for number in frameNumbers:
			convert_queue.put(number)
		
		start = time.time()
		numWorkers = 4
		workers = []
		for i in range(numWorkers):
			workers.append(Thread(target = convert_frames, args = (bits, greyscale, frameStep, 
				frameWidth, frameHeight, chunkWidth, chunkHeight, i )))
		for worker in workers:
			worker.start()
			
		movie_started = False
			
		convertFps = 0
		frameTimes = []
		frameTimeMaxLen = 64
		frameSplitStep = int((chunkWidth*chunkHeight)/4)
			
		with open('chunks.dat', 'w') as out:
			out.write("%d %d %d %d %d %d %f\n" % (bits, greyscale, frameWidth, frameHeight, chunkWidth, chunkHeight, framerate))
			out.flush()
			idx = 0
			while idx < frameCount-1:
				idx += 1
				frame = converted_chunks[idx]
				if not frame:
					time.sleep(0.01)
					idx -= 1
					continue
					
				now = int(round(time.time() * 1000))
				frameTimes.append(now)
				if len(frameTimes) > frameTimeMaxLen:
					del frameTimes[0]
				timePassed = (now - frameTimes[0])/1000.0
				if timePassed > 0:
					convertFps = int(len(frameTimes) / timePassed)				
				print("Write frame %s / %s (%s fps)" % (idx, frameCount, convertFps), end='\r')
				if not movie_started and (idx > framerate*2 or idx == frameCount-2):
					print("")
					
					playerLock.acquire()
					
					audio_source = None
					player.delete()
					player = Player()
					time.sleep(0.1)
					if os.path.exists('audio.ogg'):
						os.remove('audio.ogg')
					os.rename('temp/audio.ogg', 'audio.ogg')
					audio_source = pyglet.media.load('audio.ogg', streaming=False)
					
					playerLock.release()
					
					f2 = open(comms_path2, 'w')
					f2.write('chunks.dat %f' % framerate)
					f2.close()
					
					movie_started = True
				
				# split frames in 4ths for distributed loading in angelscript (loading from files is hella slow)
				out.write("%s\n" % frame[0:frameSplitStep])
				out.write("%s\n" % frame[frameSplitStep:frameSplitStep*2])
				out.write("%s\n" % frame[frameSplitStep*2:frameSplitStep*3])
				out.write("%s\n" % frame[frameSplitStep*3:])
				out.flush()
			
		for worker in workers:
			worker.join()
			
		print("")
		print("Finished conversion")
	except:
		traceback.print_exc()

	converting_video = False
			
def message_loop():
	global convert_queue
	global converted_chunks
	global converting_video
	global comms_path
	global comms_path2
	global cwd
	global player
	global audio_source
	
	with open(comms_path, 'r') as f:
		while True:
			line = f.readline()
			f.seek(0)
			if len(line):
				f = open(comms_path, 'w')
				f.write('')
				f.close()
				f = open(comms_path, 'r')
				if line == "play":
					try:
						#print("Playing sound")
						if not os.path.exists('audio.ogg'):
							print("Audio file is missing")
							continue
							
						playerLock.acquire()
						
						if audio_source is None:
							audio_source = pyglet.media.load('audio.ogg', streaming=False)
						player.delete()
						player = Player()
						player.queue(audio_source)
						player.play()
						
						playerLock.release()
					except:
						traceback.print_exc()
				if line.startswith("load"):
					if converting_video:
						print("Already converting video")
						continue
						
					converting_video = True
					
					args = line.split()
					url = args[1]
					bits = int(args[2])
					greyscale = args[3] != '1'
					frameWidth = int(args[4])
					frameHeight = int(args[5])
					chunkWidth = int(args[6])
					chunkHeight = int(args[7])
					
					convert_thread = Thread(target = convert_video, args = (url, bits, greyscale, frameWidth, frameHeight, chunkWidth, chunkHeight ))
					convert_thread.start()
			
def convert_frames(bits, greyscale, frameStep, frameWidth, frameHeight, chunkSizeX, chunkSizeY, workerid=0):
	global convert_queue
	global converted_chunks

	cw = 0
	ch = 0
	chunksPerFrame = 0
	while True:
		if convert_queue.empty():
			break
		frameNumber = convert_queue.get()
		
		pal = ('-map pal/pal_grey%s.bmp' % bits) if greyscale else ('-map pal/pal_gen%s.bmp' % bits)
		dither = True
		if bits >= 7:
			pal = ''
		frameTime = frameNumber*frameStep
		ffmpeg_cmd = 'ffmpeg -accurate_seek -ss %.2f -i temp/video.mp4 -vframes 1 -s %dx%d -f image2pipe -' % (frameTime, frameWidth, frameHeight)
		magick_cmd = 'gm convert - %s %s %s png:-' % ("" if dither else "+dither", "-colorspace gray" if greyscale else "", pal)
		
		output = None
		with open(os.devnull, 'w') as devnull:
			cmd = ffmpeg_cmd + ' | ' + magick_cmd
			#print('\n' + cmd + '\n')
			output = subprocess.check_output(cmd, shell=True, stderr=devnull)
		
		#img = Image.open('test.bmp')
		img = Image.open(io.BytesIO(output))
		pixels = img.convert('RGB').load()
		w, h = img.size
		
		channels = 1 if greyscale else 3 # color channels
		
		cw = int(w / chunkSizeX)
		ch = int(h / chunkSizeY)
		chunksPerFrame = cw*ch*channels
		
		chunkSizeTotal = chunkSizeX*chunkSizeY
		
		# count up but with each bit spread out to allow for interleaving values for each pixel in the chunk
		v = [0] * 256
		for i in range(255):
			n = i+1
			spread = 0
			for b in range(64):
				bit = 1 << b
				if n & bit:
					chunkGap = chunkSizeTotal*b
					#print("N has %d bit set and chunkGap is %d and b is %d" % (bit, chunkGap, b))
					spread |= 1 << chunkGap
			n = spread
			v[i+1] = n
			#print(format(n, '#032b'))
		
		# RGB + lightness dominance in this frame (is this image mostly blue/red, etc.)
		dominance = [0, 0, 0, 0]
		for py in range(h):
			for px in range(w): 
				r = pixels[px,py][0]
				g = pixels[px,py][1]
				b = pixels[px,py][2]
				dominance[0] += r
				dominance[1] += g
				dominance[2] += b
				dominance[3] += (r*0.35 + g*0.40 + b*0.25) # blue light is less bright than green/red
		
		chunks = []
		for chan in range(channels):
			for cy in range(ch):
				for cx in range(cw):
					chunkValue = 0
					pixelValue = 1 # determines which bit to set in the chunk
					numShifts = 0
					for y in range(chunkSizeY):
						for x in range(chunkSizeX):
							px = cx*chunkSizeX + x
							py = cy*chunkSizeY + y
							
							if bits == 1:
								if pixels[px, py][chan] == 255:
									chunkValue |= v[1] << numShifts
								
							elif bits == 2:
								val = pixels[px,py][chan]
								if abs(val-73) < 32:
									chunkValue |= v[1] << numShifts
								elif abs(val-180) < 32:
									chunkValue |= v[2] << numShifts
								elif val == 255:
									chunkValue |= v[3] << numShifts
								elif val != 0:
									print("Unexpected val %s" % val)
									
							elif bits == 3:
								val = pixels[px,py][chan]
								step = 36.43
								totalSteps = int((255.0 / step) + 0.5)
								invalid = True
								for k in range(totalSteps):
									if abs(val-step*(k+1)) < 24:
										chunkValue |= v[k+1] << numShifts
										invalid = False
								if val != 0 and invalid:
									print("Unexpected val %s" % val)
									
							elif bits == 4:
								val = pixels[px,py][chan]
								step = 17
								totalSteps = int((255.0 / step) + 0.5)
								invalid = True
								for k in range(totalSteps):
									if abs(val-step*(k+1)) < 8:
										chunkValue |= v[k+1] << numShifts
										invalid = False
								if val != 0 and invalid:
									print("Unexpected val %s" % val)

							elif bits == 5:
								val = pixels[px,py][chan]
								step = 8.22
								totalSteps = int((255.0 / step) + 0.5)
								invalid = True
								for k in range(totalSteps):
									if abs(val-step*(k+1)) < 4:
										chunkValue |= v[k+1] << numShifts
										invalid = False
								if val != 0 and invalid:
									print("Unexpected val %s" % val)
								
							elif bits == 6:
								val = pixels[px,py][chan]
								step = 4.05
								totalSteps = int((255.0 / step) + 0.5)
								invalid = True
								for k in range(totalSteps):
									if abs(val-step*(k+1)) < 3:
										chunkValue |= v[k+1] << numShifts
										invalid = False
								if val != 0 and invalid:
									print("Unexpected val %s" % val)
									
							elif bits == 7:
								val = int(pixels[px,py][chan] / 2)
								chunkValue |= v[val] << numShifts
									
							elif bits == 8:
								val = pixels[px,py][chan]
								chunkValue |= v[val] << numShifts
							
							pixelValue <<= 1
							numShifts += 1
									
					chunks.append(chunkValue)
					#print("GET VAL %d" % chunkValue)
					
		maxBright = 255*w*h*3
		for i in range(4):
			dominance[i] = int(255*(float(dominance[i]) / float(maxBright)))
		chunks.append((dominance[0] << 24) | (dominance[1] << 16) | (dominance[2] << 8) | dominance[3])
					
		chunk_values = ''
		for chunk in chunks:
			chunk_values += "%d " % chunk
		chunk_values = chunk_values[:-1]
		converted_chunks[frameNumber] = chunk_values
		
	convert_queue.task_done()
		
message_loop()
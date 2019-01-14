import pygame
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
			
# work shared between threads
convert_queue = Queue()
converted_chunks = []

def download_video(yt):
	print("Downloading video stream: %s" % yt.streams.filter(adaptive=True, subtype='mp4', res='144p').first())
	video = yt.streams.filter(adaptive=True, subtype='mp4', res='144p').first().download(filename='video')
	print("Finished video download")

def download_audio(yt):
	print("Downloading audio stream: %s" % yt.streams.filter(only_audio=True, audio_codec='vorbis').order_by('abr').asc().first())
	audio = yt.streams.filter(only_audio=True, audio_codec='vorbis').order_by('abr').asc().first().download(filename='audio')
	print("Finished audio download")
			
def message_loop():
	global convert_queue
	global converted_chunks
	
	pygame.mixer.init()

	cwd = os.getcwd()
	
	comms_path = "../../../../svencoop/scripts/maps/temp/sound_comms"
	comms_path2 = "../../../../svencoop/scripts/maps/temp/sound_comms2"
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
						print("LOADING SOUND")
						pygame.mixer.music.load('audio.ogg')
						print("Playing sound")
						pygame.mixer.music.play()
					except:
						traceback.print_exc()
				if line.startswith("load"):
					framerate = 30.0;
					try:
						print("GOT: " + line)
						
						pygame.mixer.quit()
						pygame.mixer.init()
						if os.path.exists('audio.ogg'):
							os.remove('audio.ogg')
						
						url = line.split()[1]
						bits = int(line.split()[2])
						greyscale = line.split()[3] != '1'
						
						print("GREYSCALE? %s" % greyscale)
						
						print("Querying " + url)
						yt = YouTube(url)
						print("Video: %s (%s)" % (yt.title, yt.length))
						
						#print("Video streams:")
						#pprint(yt.streams.filter(adaptive=True, subtype='mp4', res='144p').all())
						#print("Audio streams:")
						#pprint(yt.streams.filter(only_audio=True).order_by('abr').asc().all())

						os.chdir('temp')
						
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
						print("Converting audio stream")
						if os.path.exists('audio.ogg'):
							os.remove('audio.ogg')
						system("ffmpeg -i audio.webm audio.ogg")
						
						
						frame_ext = '.jpg'
						for file in os.listdir('.'):
							if frame_ext not in file or 'temp' in file or 'pal' in file:
								continue
							os.remove(file)
						
						fname = 'video.mp4'
						print("Extracting frames from %s" % fname)
						system("magick convert " + fname + " %d.jpg")
						print("Converting frames")
						
						names = []
						for file in os.listdir('.'):
							if frame_ext not in file or 'temp' in file or 'pal' in file:
								continue
							names.append(file[:file.find(frame_ext)])
						names = sorted(names, key=int)
						#names = names[:1000]
						converted_chunks = [''] * len(names)
						
						convert_queue = Queue()
						for name in names:
							convert_queue.put(name)
						
						start = time.time()
						numWorkers = 4
						workers = []
						for i in range(numWorkers):
							workers.append(Thread(target = convert_frames, args = (bits, greyscale, i )))
						for worker in workers:
							worker.start()
							
						with open('../chunks.dat', 'w') as out:
							idx = 0
							while idx < len(names)-1:
								idx += 1
								frame = converted_chunks[idx]
								if not frame:
									#print("Waiting for frame %s" % idx)
									time.sleep(0.01)
									idx -= 1
									continue
								print("Write frame %s / %s" % (idx, len(names)))
								out.write("%s\n" % frame)
							
						for worker in workers:
							worker.join()
							
						framerate = float(len(names)) / float(yt.length)
						print("Duration %s" % (time.time() - start))						
						convert_frames()
					except Exception as e:
						traceback.print_exc()
					finally:
						os.chdir(cwd)
					
					os.rename('temp/audio.ogg', 'audio.ogg')
						
					f2 = open(comms_path2, 'w')
					f2.write('chunks.dat %f' % framerate)
					f2.close()
			
def convert_frames(bits=3, greyscale=True, workerid=0):
	global convert_queue
	global converted_chunks
	
	frame_ext = '.jpg'
	
	chunkSizeX = 6
	chunkSizeY = 3

	if bits == 1:
		chunkSizeX = 6
		chunkSizeY = 3
	if bits == 2:
		chunkSizeX = 3
		chunkSizeY = 3
	if bits == 3:
		chunkSizeX = 3
		chunkSizeY = 2
	if bits == 4:
		chunkSizeX = 2
		chunkSizeY = 2
	if bits == 6:
		chunkSizeX = 3
		chunkSizeY = 1
	if bits == 8:
		chunkSizeX = 2
		chunkSizeY = 1

	cw = 0
	ch = 0
	chunksPerFrame = 0
	tempFname = "temp%d.bmp" % workerid
	while True:
		if convert_queue.empty():
			break
		name = convert_queue.get()
		
		fname = name + frame_ext
		#print("Convert " + fname)
		
		if os.path.exists(tempFname):
			os.remove(tempFname)
		
		if False:
			#system("magick convert %s -resize 72x42! -colorspace gray -contrast-stretch 0 -remap ../pal/pal_grey3.bmp %s" % (fname, tempFname))
			system("gm convert %s -type Palette -colorspace gray -operator matte negate 1 -resize 72x42! -map pal_grey3.bmp -compress none %s" % (fname, tempFname))
		elif greyscale:
			if bits == 1:
				system("magick convert -colorspace gray -resize 126x72! -contrast-stretch 0 -monochrome " + fname + " " + tempFname)
			elif bits == 2:
				system("magick convert -colorspace gray -resize 87x51! -contrast-stretch 0 -remap ../pal/pal_grey2.bmp " + fname + " " + tempFname)
			elif bits == 3:
				system("magick convert %s -resize 72x42! -colorspace gray -contrast-stretch 0 -remap ../pal/pal_grey3.bmp %s" % (fname, tempFname))
			elif bits == 6:
				system("magick convert %s -resize 51x30! -colorspace gray -contrast-stretch 0 -remap ../pal/pal_grey6.bmp %s" % (fname, tempFname))
			elif bits == 8:
				system("magick convert %s -resize 42x24! -colorspace gray %s" % (fname, tempFname))
		else:
			if bits == 8:
				system("magick convert %s -resize 24x14! %s" % (fname, tempFname))
			if bits == 6:
				system("magick convert %s -resize 30x16! -remap ../pal/pal_gen6.bmp %s" % (fname, tempFname))
			if bits == 4:
				system("magick convert %s -resize 34x20! -remap ../pal/pal_gen4.bmp %s" % (fname, tempFname))
			if bits == 3:
				system("magick convert %s -resize 42x24! -remap ../pal/pal_gen3.bmp %s" % (fname, tempFname))
			if bits == 2:
				system("magick convert %s -resize 51x30! -remap ../pal/pal_gen2.bmp -type truecolor +dither %s" % (fname, tempFname))
			if bits == 1:
				system("magick convert %s -resize 72x42! -remap ../pal/pal_gen1.bmp -type truecolor +dither %s" % (fname, tempFname))
		
		img = Image.open(tempFname)
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
									
							elif bits == 8:
								val = pixels[px,py][chan]
								chunkValue |= v[val] << numShifts
							pixelValue <<= 1
							numShifts += 1
									
					chunks.append(chunkValue)
					#print("GET VAL %d" % chunkValue)
					
		chunk_values = ''
		for chunk in chunks:
			chunk_values += "%d " % chunk
		chunk_values = chunk_values[:-1]
		converted_chunks[int(name)] = chunk_values
		
	convert_queue.task_done()
		
message_loop()
from PIL import Image
from os import system
import os

names = []
for file in os.listdir('.'):
	if '.bmp' not in file or 'temp' in file or 'pal' in file:
		continue
	names.append(file[:file.find(".bmp")])

names = sorted(names, key=int)

chunkSizeX = 6
chunkSizeY = 3

bits = 8

if bits == 4:
	chunkSizeX = 3
	chunkSizeY = 3
if bits == 8:
	chunkSizeX = 3
	chunkSizeY = 2

cw = 0
ch = 0
chunksPerFrame = 0
chunks = []
tempFname = "temp.bmp"
for name in names:
	fname = name + ".bmp"
	
	print("Convert " + fname)
	if os.path.exists(tempFname):
		os.remove(tempFname)
		
	if bits == 2:
		system("magick convert -colorspace gray -resize 126x72! -contrast-stretch 0 -monochrome " + fname + " " + tempFname)
	elif bits == 4:
		system("magick convert -colorspace gray -resize 87x51! -colors 5 -contrast-stretch 0  " + fname + " " + tempFname)
	elif bits == 8:
		system("magick convert %s -resize 72x42! -colorspace gray -contrast-stretch 0 -remap pal.bmp %s" % (fname, tempFname))
	
	img = Image.open("temp.bmp")
	pixels = img.load()
	w, h = img.size
	
	cw = int(w / chunkSizeX)
	ch = int(h / chunkSizeY)
	chunksPerFrame = cw*ch
	
	for cy in range(ch):
		for cx in range(cw):
			chunkValue = 0
			pixelValue = 1
			for y in range(chunkSizeY):
				for x in range(chunkSizeX):
					px = cx*chunkSizeX + x
					py = cy*chunkSizeY + y
					if bits == 2:
						if pixels[px, py][0] == 255:
							chunkValue |= pixelValue
						
					elif bits == 4:
						val = pixels[px,py][0]
						if abs(val-73) < 32:
							chunkValue |= pixelValue
						elif abs(val-180) < 32:
							chunkValue |= pixelValue << ((chunkSizeX*chunkSizeY))
						elif val == 255:
							chunkValue |= pixelValue << ((chunkSizeX*chunkSizeY)) | pixelValue
						elif val != 0:
							print("Unexpected val %s" % val)
							
					elif bits == 8:
						val = pixels[px,py][0]
						if abs(val-35.5) < 24:
							chunkValue |= pixelValue
						elif abs(val-71) < 24:
							chunkValue |= (pixelValue << (chunkSizeX*chunkSizeY))
						elif abs(val-106.5) < 24:
							chunkValue |= (pixelValue << (chunkSizeX*chunkSizeY)) | pixelValue
						elif abs(val-142) < 24:
							chunkValue |= (pixelValue << (chunkSizeX*chunkSizeY*2))
						elif abs(val-177.5) < 24:
							chunkValue |= (pixelValue << (chunkSizeX*chunkSizeY*2)) | pixelValue
						elif abs(val-213) < 24:
							chunkValue |= (pixelValue << (chunkSizeX*chunkSizeY*2)) | (pixelValue << (chunkSizeX*chunkSizeY))
						elif val == 255:
							chunkValue |= (pixelValue << (chunkSizeX*chunkSizeY*2)) | (pixelValue << (chunkSizeX*chunkSizeY)) | pixelValue
						elif val != 0:
							print("Unexpected val %s" % val)
					pixelValue <<= 1
						
			chunks.append(chunkValue)

with open('chunks.dat', 'w') as f:
	for idx, chunk in enumerate(chunks):
		f.write("%s" % chunk)
		if (idx+1) % chunksPerFrame == 0 and idx != 0:
			f.write("\n")
		else:
			f.write(" ")

	print("Wrote %d %sx%s frames" % (len(chunks)/chunksPerFrame, cw, ch))
from PIL import Image
from os import system
import os

names = []
for file in os.listdir('.'):
	if '.bmp' not in file or 'temp' in file or 'pal' in file:
		continue
	names.append(file[:file.find(".bmp")])

names = sorted(names, key=int)

greyscale = False

chunkSizeX = 6
chunkSizeY = 3

bits = 2

if bits == 2:
	chunkSizeX = 6
	chunkSizeY = 3
if bits == 4:
	chunkSizeX = 3
	chunkSizeY = 3
if bits == 8:
	chunkSizeX = 3
	chunkSizeY = 2

# Generate a rgb palette with 3 bits per color (8 possible shades)
if bits == 8:
	pgen = Image.new('RGB', (32, 16), color = 'black')
	pix = pgen.load()
	r = 0
	g = 0
	b = 0
	cstep = 36.43
	for y in range(16):
		for x in range(32):
			pix[x,y] = (int(r),int(g),int(b))
			r += cstep
			if r > 256:
				g += cstep
				r = 0
				if g > 256:
					b += cstep
					g = 0
	pgen.save("pal_gen8.bmp")
if bits == 4:
	pgen = Image.new('RGB', (8, 8), color = 'black')
	pix = pgen.load()
	r = 0
	g = 0
	b = 0
	cstep = 85
	for y in range(8):
		for x in range(8):
			pix[x,y] = (int(r),int(g),int(b))
			r += cstep
			if r > 256:
				g += cstep
				r = 0
				if g > 256:
					b += cstep
					g = 0
	pgen.save("pal_gen4.bmp")
if bits == 2:
	pgen = Image.new('RGB', (4, 2), color = 'black')
	pix = pgen.load()
	r = 0
	g = 0
	b = 0
	cstep = 255
	for y in range(2):
		for x in range(4):
			pix[x,y] = (int(r),int(g),int(b))
			r += cstep
			if r > 256:
				g += cstep
				r = 0
				if g > 256:
					b += cstep
					g = 0
	pgen.save("pal_gen2.bmp")

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
		
	if greyscale:
		if bits == 2:
			system("magick convert -colorspace gray -resize 126x72! -contrast-stretch 0 -monochrome " + fname + " " + tempFname)
		elif bits == 4:
			system("magick convert -colorspace gray -resize 87x51! -colors 5 -contrast-stretch 0  " + fname + " " + tempFname)
		elif bits == 8:
			system("magick convert %s -resize 72x42! -colorspace gray -contrast-stretch 0 -remap pal.bmp %s" % (fname, tempFname))
	else:
		if bits == 8:
			system("magick convert %s -resize 42x24! -remap pal_gen8.bmp %s" % (fname, tempFname))
		if bits == 4:
			system("magick convert %s -resize 51x30! -remap pal_gen4.bmp -type truecolor +dither %s" % (fname, tempFname))
		if bits == 2:
			system("magick convert %s -resize 72x42! -remap pal_gen2.bmp -type truecolor +dither %s" % (fname, tempFname))
	
	img = Image.open("temp.bmp")
	pixels = img.load()
	w, h = img.size
	
	channels = 1 if greyscale else 3 # color channels
	
	cw = int(w / chunkSizeX)
	ch = int(h / chunkSizeY)
	chunksPerFrame = cw*ch*channels
	
	chunkSizeTotal = chunkSizeX*chunkSizeY
	for chan in range(channels):
		for cy in range(ch):
			for cx in range(cw):
				chunkValue = 0
				pixelValue = 1 # determines which bit to set in the chunk
				for y in range(chunkSizeY):
					for x in range(chunkSizeX):
						px = cx*chunkSizeX + x
						py = cy*chunkSizeY + y
						
						v1 = pixelValue
						v2 = v1 << (chunkSizeTotal)
						v3 = v2 | v1
						v4 = v1 << (chunkSizeTotal*2)
						v5 = v4 | v1
						v6 = v4 | v2
						v7 = v4 | v2 | v1
						
						if bits == 2:
							if pixels[px, py][chan] == 255:
								chunkValue |= v1
							
						elif bits == 4:
							val = pixels[px,py][chan]
							if abs(val-73) < 32:
								chunkValue |= v1
							elif abs(val-180) < 32:
								chunkValue |= v2
							elif val == 255:
								chunkValue |= v3
							elif val != 0:
								print("Unexpected val %s" % val)
								
						elif bits == 8:
							val = pixels[px,py][chan]
							if abs(val-35.5) < 24:
								chunkValue |= v1
							elif abs(val-71) < 24:
								chunkValue |= v2
							elif abs(val-106.5) < 24:
								chunkValue |= v3
							elif abs(val-142) < 24:
								chunkValue |= v4
							elif abs(val-177.5) < 24:
								chunkValue |= v5
							elif abs(val-213) < 24:
								chunkValue |= v6
							elif val == 255:
								chunkValue |= v7
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
	print("chunks %d and chunksPerFrame %s " % (len(chunks), chunksPerFrame))
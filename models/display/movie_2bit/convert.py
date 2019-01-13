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

bits = 8

if bits == 1:
	chunkSizeX = 6
	chunkSizeY = 3
if bits == 2:
	chunkSizeX = 3
	chunkSizeY = 3
if bits == 3:
	chunkSizeX = 3
	chunkSizeY = 2
if bits == 6:
	chunkSizeX = 3
	chunkSizeY = 1
if bits == 8:
	chunkSizeX = 2
	chunkSizeY = 1

# Generate a rgb palette with 3 bits per color (8 possible shades)
if bits == 6:
	pgen = Image.new('RGB', (512, 512), color = 'black')
	pix = pgen.load()
	r = 0
	g = 0
	b = 0
	cstep = 4.05
	for y in range(512):
		for x in range(512):
			pix[x,y] = (int(r),int(g),int(b))
			r += cstep
			if r > 256:
				g += cstep
				r = 0
				if g > 256:
					b += cstep
					g = 0
	pgen.save("pal_gen6.bmp")
if bits == 3:
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
	pgen.save("pal_gen3.bmp")
if bits == 2:
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
	pgen.save("pal_gen2.bmp")
if bits == 1:
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
	pgen.save("pal_gen1.bmp")

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
		if bits == 1:
			system("magick convert -colorspace gray -resize 126x72! -contrast-stretch 0 -monochrome " + fname + " " + tempFname)
		elif bits == 2:
			system("magick convert -colorspace gray -resize 87x51! -colors 5 -contrast-stretch 0  " + fname + " " + tempFname)
		elif bits == 3:
			system("magick convert %s -resize 72x42! -colorspace gray -contrast-stretch 0 -remap pal.bmp %s" % (fname, tempFname))
	else:
	    # 24x14
		if bits == 8:
			system("magick convert %s -resize 24x14! %s" % (fname, tempFname))
		if bits == 6:
			system("magick convert %s -resize 30x16! -remap pal_gen6.bmp %s" % (fname, tempFname))
		if bits == 3:
			system("magick convert %s -resize 42x24! -remap pal_gen3.bmp %s" % (fname, tempFname))
		if bits == 2:
			system("magick convert %s -resize 51x30! -remap pal_gen2.bmp -type truecolor +dither %s" % (fname, tempFname))
		if bits == 1:
			system("magick convert %s -resize 72x42! -remap pal_gen1.bmp -type truecolor +dither %s" % (fname, tempFname))
	
	img = Image.open("temp.bmp")
	pixels = img.load()
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

with open('chunks.dat', 'w') as f:
	for idx, chunk in enumerate(chunks):
		f.write("%s" % chunk)
		if (idx+1) % chunksPerFrame == 0 and idx != 0:
			f.write("\n")
		else:
			f.write(" ")

	print("Wrote %d %sx%s frames" % (len(chunks)/chunksPerFrame, cw, ch))
	print("chunks %d and chunksPerFrame %s " % (len(chunks), chunksPerFrame))
from PIL import Image
from os import system
import math

bitsPerPixel = 128
tint = (1,1,1)

w = 0
h = 0

if bitsPerPixel == 256: # actually 8 bit
	w = 3
	h = 1
if bitsPerPixel == 128: # actually 6 bit
	w = 3
	h = 1
if bitsPerPixel == 64: # actually 6 bit
	w = 3
	h = 1
if bitsPerPixel == 32: # actually 5 bit
	w = 2
	h = 2
if bitsPerPixel == 16: # actually 4 bit
	w = 5
	h = 1
if bitsPerPixel == 8: # actually 3 bit
	w = 7
	h = 1
if bitsPerPixel == 4: # actually 2 bit
	w = 5
	h = 2
if bitsPerPixel == 2: # actually 1 bit
	w = 6
	h = 3
	
pad = 0 # bilinear filtering will make chunk borders obvious without this

polyGap = 256
polyDim = 256
halfTotalGap = (polyDim*polyGap) / 2

maxSkinsPerMdl = 256
maxModelBodies = 256
masterChunkDim = int(math.sqrt(maxModelBodies*polyDim))
masterChunkDim = 256
#masterW = (w+pad)*256 + pad
#masterH = (h+pad)*256 + pad

# combos = 43520
# 170*256

combos = bitsPerPixel**(w*h)

masterW = w*64
masterH = h*128
combosPerSkin = int(masterW / w) * int(masterH / h)
polyDim = math.ceil(combosPerSkin / maxModelBodies)
bodiesPerSkin = min(256, combos / polyDim)
framesLastBody = combosPerSkin % (polyDim)
totalSkins = math.ceil(combos / combosPerSkin)
if framesLastBody == 0:
	framesLastBody = polyDim

combosLastSkin = combos % combosPerSkin
bodiesLastSkin = math.ceil(combosLastSkin / polyDim)
if bodiesLastSkin == 0:
	bodiesLastSkin = bodiesPerSkin
framesLastSkinAndBody = (combos % combosPerSkin) % polyDim

print("Total skins: %s" % totalSkins)
print("Combos per skin: %s" % combosPerSkin)
print("")
print("Bodies per skin: %s" % bodiesPerSkin)
print("Bodies last skin: %s" % bodiesLastSkin)
print("Frames per body: %s" % (polyDim))
print("Frames last body: %s" % framesLastBody)
print("Frames last skin+body: %s" % framesLastSkinAndBody)
print("Skins last model: %s" % (totalSkins % maxSkinsPerMdl))

print("Master tex size: %sx%s" % (masterW, masterH))

masterScale = 1

print("possible combos %s" % combos)

args = ''

img = Image.new('RGB', (masterW, masterH))
pixels = img.load()

smd_header = '''version 1
nodes
0 "joint1" -1
end
skeleton
time 0
0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000
end
triangles'''

qc_start = '''
$cd "."
$cdtexture "."
$scale 1.0

$flags 0

$sequence "pan" {
	"smd/pan"
	fps 0
}

$bodygroup "body"
{
'''

anim_header = '''version 1
nodes
0 "joint1" -1
end
skeleton
'''
with open('smd/pan.smd', 'w') as f:
	f.write(anim_header)
	time = 0
			
	if False:
		# this somehow results in worse jitter than a 1-dimensional animation with higher floating point errors			
		for pz in range(polyDim):
			for py in range(polyDim):
				for px in range(polyDim):
					#double frame to prevent not transitioning completley when incrementing frame
					f.write('time %d\n' % time)
					f.write('0 %.6f %.6f %.6f 0.000000 0.000000 0.000000\n' % (-px*polyGap, -pz*polyGap, -py*polyGap))
					time += 1
					f.write('time %d\n' % time)
					f.write('0 %.6f %.6f %.6f 0.000000 0.000000 0.000000\n' % (-px*polyGap, -pz*polyGap, -py*polyGap))
					time += 1
		maxDist = (polyDim-1)*polyGap
		while time < 256:
			f.write('time %d\n' % time)
			f.write('0 %.6f %.6f %.6f 0.000000 0.000000 0.000000\n' % (-maxDist, -maxDist, -maxDist))
			time += 1
			
	else:
		max = 256*polyGap - halfTotalGap
		f.write('time %d\n' % time)
		f.write('0 %.6f %.6f %.6f 0.000000 0.000000 0.000000\n' % (halfTotalGap, 0, 0))
		time += 1
		f.write('time %d\n' % time)
		f.write('0 %.6f %.6f %.6f 0.000000 0.000000 0.000000\n' % (-max, 0, 0))
		time += 1
	f.write('end\n')

masterX = 0
masterY = 0
masterIdx = 0
mdlIdx = 0
smdWrite = 0
polyX = 0
smd = open('smd/0.smd', 'w')
smd.write(smd_header + "\n")
offsetX = 0
offsetY = 0
wroteAllMeshesForModel = False

def getTexName(idx):
	#return "Remap%s_000_255_255" % (idx+1)
	#return "DM_Base"
	return "%s" % idx

inc = 256/(bitsPerPixel-1)
for combo in range(0, combos):
	bits = []
				
	offsetX = masterX*(w+pad) + pad
	offsetY = masterY*(h+pad) + pad
	
	for level in range(bitsPerPixel):
		inc_level = inc*(1 << level)
		for y in range(0, h):
			for x in range(0, w):
				b = (y*w + x) + level*w*h
				px = offsetX + x
				py = offsetY + y
				if combo & (1 << b) != 0:
					mr = int( min(pixels[px,py][0] + inc_level, 255) )
					mg = int( min(pixels[px,py][1] + inc_level, 255) )
					mb = int( min(pixels[px,py][2] + inc_level, 255) )
					pixels[px,py] = (mr,mg,mb)

					if pad > 0 and bitsPerPixel == 2:
						# draw pixels on borders to prevent bilinear filtering to an adjacent pixel outside of the chunk
						if x == 0: pixels[px-1,py] = (255,255,255)
						if y == 0: pixels[px,py-1] = (255,255,255)
						if x == w-1: pixels[px+1,py] = (255,255,255)
						if y == h-1: pixels[px,py+1] = (255,255,255)
						
						# top left corner
						if x == 0 and y == 0: pixels[px-1,py-1] = (255,255,255)
						# top right corner
						if x == w-1 and y == 0: pixels[px+1,py-1] = (255,255,255)
						# bottom left corner
						if x == 0 and y == h-1: pixels[px-1,py+1] = (255,255,255)
						# bottom right corner
						if x == w-1 and y == h-1: pixels[px+1,py+1] = (255,255,255)
	
	if not wroteAllMeshesForModel:
		f = smd
		
		u1 = offsetX / masterW
		u2 = (offsetX+w) / masterW
		v2 = 1 - (offsetY / masterH)
		v1 = 1 - ((offsetY+h) / masterH)
		
		if False:
			for pz in range(polyDim):
				for py in range(polyDim):
					for px in range(polyDim):
						ofsX = px*polyGap
						ofsY = py*polyGap
						ofsZ = pz*polyGap
						f.write("%s.bmp\n" % getTexName(mdlIdx*maxSkinsPerMdl))
						f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX, ofsZ, ofsY, u1, v2))
						f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX, ofsZ, ofsY-h, u1, v1))
						f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX+w, ofsZ, ofsY-h, u2, v1))
						f.write("%s.bmp\n" % getTexName(mdlIdx*maxSkinsPerMdl))
						f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX, ofsZ, ofsY, u1, v2))
						f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX+w, ofsZ, ofsY-h, u2, v1))
						f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX+w, ofsZ, ofsY, u2, v2))
		else:			
			ofsX = polyX*polyGap - halfTotalGap
			ofsZ = -256
			f.write("%s.bmp\n" % getTexName(mdlIdx*maxSkinsPerMdl))
			f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX, ofsZ, 0, u1, v2))
			f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX, ofsZ, 0-h, u1, v1))
			f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX+w, ofsZ, 0-h, u2, v1))
			f.write("%s.bmp\n" % getTexName(mdlIdx*maxSkinsPerMdl))
			f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX, ofsZ, 0, u1, v2))
			f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX+w, ofsZ, 0-h, u2, v1))
			f.write('0 %.6f %.6f %.6f 1.000000 0.000000 1.000000 %.6f %.6f\n' % (ofsX+w, ofsZ, 0, u2, v2))
			
			polyX += 1
			
			if polyX >= polyDim:
				polyX = 0
				smdWrite += 1
				
				smd.write('end\n')
				smd = open('smd/%s.smd' % smdWrite, 'w')
				smd.write(smd_header + "\n")	

	isLast = combo == combos-1
	masterX += 1
	if isLast or masterX*(w+pad) + w > masterW:
		masterX = 0
		masterY += 1
		if isLast or masterY*(h+pad) + h > masterH:
			print("Master idx %s filled at %s" % (masterIdx, combo))

			if not wroteAllMeshesForModel:
				wroteAllMeshesForModel = True
				print("End mesh generation")
				smd.write('end\n')
				smd.close()
			
			masterX = masterY = 0
			img = img.resize((masterW*masterScale, masterH*masterScale))
			for y in range(masterH):
				for x in range(masterW):
					pixels[x,y] = (pixels[x,y][0]*tint[0], pixels[x,y][1]*tint[1], pixels[x,y][2]*tint[2])
			img = img.quantize()
			img.save("%s.bmp" % getTexName(masterIdx))
			img = Image.new('RGB', (masterW, masterH))
			pixels = img.load()
			masterIdx += 1
			
			if isLast or masterIdx % maxSkinsPerMdl == 0:
				if isLast and not wroteAllMeshesForModel:
					smd.write('end\n')
					smd.close()
				wroteAllMeshesForModel = False
				polyX = 0
				qcname = 'd%s.qc' % mdlIdx
				with open(qcname, 'w') as f:
					f.write('$modelname "%s.mdl"' % mdlIdx)
					f.write(qc_start)
					for x in range(smdWrite):
						f.write('\tstudio "smd/%s"\n' % x)	
					f.write("}\n\n")
					f.write('$texturegroup "skinfamilies"\n')
					f.write('{\n')
					for x in range(mdlIdx*maxSkinsPerMdl, min(mdlIdx*maxSkinsPerMdl + maxSkinsPerMdl, masterIdx)):
						if x > masterIdx:
							break
						f.write('\t{ "%s.bmp" }\n' % getTexName(x))	
					f.write("}\n")
					
					
					for x in range(mdlIdx*maxSkinsPerMdl, min(mdlIdx*maxSkinsPerMdl + maxSkinsPerMdl, masterIdx)):
						if x > masterIdx:
							break
						#f.write('$texrendermode "%s.bmp" "fullbright"\n' % getTexName(x))
						#f.write('$texrendermode "%s.bmp" "flatshade"\n' % getTexName(x))
						f.write('$texrendermode "%s.bmp" "additive"\n' % getTexName(x))
						
				smdWrite = 0	
				system('studiomdl.exe ' + qcname)
				#system("pause")
				
				smd = open('smd/%s.smd' % smdWrite, 'w')
				smd.write(smd_header + "\n")
				
				mdlIdx += 1

	
#img = img.resize((masterW*masterScale, masterH*masterScale))
#img.save("%s.bmp" % masterIdx)

#system("magick mogrify -resize 32x32 -filter box *.bmp")
	
#system("magick mogrify -resize 32x32 -filter box *.png")
#system("spriteguy.exe %s test.spr" % args)
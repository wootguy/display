from PIL import Image
from os import system

bitsPerPixel = 256
tint = (0,0,1)

w = 0
h = 0

if bitsPerPixel == 256: # actually 8 bit
	w = 2
	h = 1
if bitsPerPixel == 64: # actually 6 bit
	w = 3
	h = 1
if bitsPerPixel == 8: # actually 3 bit
	w = 3
	h = 2
if bitsPerPixel == 4: # actually 2 bit
	w = 3
	h = 3
if bitsPerPixel == 2: # actually 1 bit
	w = 6
	h = 3
	
pad = 0 # bilinear filtering will make chunk borders obvious without this

masterW = (w+pad)*16 + pad
masterH = (h+pad)*16 + pad

print("Master tex size: %sx%s" % (masterW, masterH))

masterScale = 1


combos = bitsPerPixel**(w*h)


maxSkinsPerMdl = 100

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
0 0.000000 -0.000000 -1.000000 1.570796 -0.000000 0.000000
end
triangles'''

qc_start = '''
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$flags 0

$sequence "idle" {
	"idle"
	fps 1
}

$bodygroup "body"
{
'''

masterX = 0
masterY = 0
masterIdx = 0
mdlIdx = 0
smdWrite = 0
for combo in range(1, combos+1):
	bits = []
	
	shouldNewSkin = combo != 0 and combo % 256 == 0
	isLast = combo == combos
	masterX += 1
	if isLast or shouldNewSkin or masterX*(w+pad) + w > masterW:
		masterX = 0
		masterY += 1
		if isLast or shouldNewSkin or masterY*(h+pad) + h > masterH:
			print("Master idx %s filled at %s" % (masterIdx, combo))
			masterX = masterY = 0
			img = img.resize((masterW*masterScale, masterH*masterScale))
			for y in range(masterH):
				for x in range(masterW):
					pixels[x,y] = (pixels[x,y][0]*tint[0], pixels[x,y][1]*tint[1], pixels[x,y][2]*tint[2])
			img = img.quantize()
			img.save("%s.bmp" % masterIdx)
			img = Image.new('RGB', (masterW, masterH))
			pixels = img.load()
			masterIdx += 1
			if isLast or masterIdx % maxSkinsPerMdl == 0:
				smdWrite = 0
				qcname = 'd%s.qc' % mdlIdx
				with open(qcname, 'w') as f:
					f.write('$modelname "%s.mdl"' % mdlIdx)
					f.write(qc_start)
					for x in range(0, min(combos, 256)):
						f.write('\tstudio "smd/%s"\n' % x)	
					f.write("}\n\n")
					f.write('$texturegroup "skinfamilies"\n')
					f.write('{\n')
					for x in range(mdlIdx*maxSkinsPerMdl, min(mdlIdx*maxSkinsPerMdl + maxSkinsPerMdl, masterIdx)):
						if x > masterIdx:
							break
						f.write('\t{ "%s.bmp" }\n' % x)	
					f.write("}\n")
					
					
					for x in range(mdlIdx*maxSkinsPerMdl, min(mdlIdx*maxSkinsPerMdl + maxSkinsPerMdl, masterIdx)):
						if x > masterIdx:
							break
						f.write('$texrendermode "%s.bmp" "fullbright"\n' % x)
				
				system('studiomdl.exe ' + qcname)
				#system("pause")
	
				mdlIdx += 1
	offsetX = masterX*(w+pad) + pad
	offsetY = masterY*(h+pad) + pad
	
	inc = 256/(bitsPerPixel-1)
	for level in range(bitsPerPixel):
		for y in range(0, h):
			for x in range(0, w):
				b = (y*w + x) + level*w*h
				px = offsetX + x
				py = offsetY + y
				if combo & (1 << b) != 0:
					mr = int( min(pixels[px,py][0] + inc*(1 << level), 255) )
					mg = int( min(pixels[px,py][1] + inc*(1 << level), 255) )
					mb = int( min(pixels[px,py][2] + inc*(1 << level), 255) )
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
	
	if smdWrite < 256:
		smdWrite += 1
		with open('smd/%s.smd' % (combo % 256), 'w') as f:
			u1 = offsetX / masterW
			u2 = (offsetX+w) / masterW
			v2 = 1 - (offsetY / masterH)
			v1 = 1 - ((offsetY+h) / masterH)
			
			f.write(smd_header + "\n")
			f.write("%s.bmp\n" % (mdlIdx*maxSkinsPerMdl))			
			f.write('0 0.000000 -0.000000 0.000000 1.000000 -0.000000 0.000000 %.6f %.6f\n' % (u1, v2))
			f.write('0 0.000000 -0.000000 %.6f 1.000000 -0.000000 0.000000 %.6f %.6f\n' % (-h, u1, v1))
			f.write('0 %.6f 0.000000 %.6f 1.000000 -0.000000 0.000000 %.6f %.6f\n' % (w, -h, u2, v1))
			f.write("%s.bmp\n" % (mdlIdx*maxSkinsPerMdl))			
			f.write('0 0.000000 -0.000000 0.000000 1.000000 -0.000000 0.000000 %.6f %.6f\n' % (u1, v2))
			f.write('0 %.6f 0.000000 %.6f 1.000000 -0.000000 0.000000 %.6f %.6f\n' % (w, -h, u2, v1))
			f.write('0 %.6f 0.000000 0.000000 1.000000 -0.000000 0.000000 %.6f %.6f\n' % (w, u2, v2))
			f.write('end\n')
	
#img = img.resize((masterW*masterScale, masterH*masterScale))
#img.save("%s.bmp" % masterIdx)

#system("magick mogrify -resize 32x32 -filter box *.bmp")
	
#system("magick mogrify -resize 32x32 -filter box *.png")
#system("spriteguy.exe %s test.spr" % args)
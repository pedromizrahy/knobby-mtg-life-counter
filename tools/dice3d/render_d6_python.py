"""Build six result-specific D6 3D roll paths without Blender (Pillow, NumPy, SciPy, OpenCV)."""
import math,pathlib,sys
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont
from scipy.spatial.transform import Rotation,Slerp
W=176;S=W*3;N=8;C=S/2
OUT=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'knobby/src/dial_d6_frames.h')
# Use the same bundled/OS-available outline font family on Linux or Windows.
font_candidates=[
 pathlib.Path(__file__).resolve().parent/'DejaVuSans-Bold.ttf',
 pathlib.Path('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'),
 pathlib.Path('C:/Windows/Fonts/arialbd.ttf'),
 pathlib.Path('C:/Windows/Fonts/seguisb.ttf'),
]
font_path=next((p for p in font_candidates if p.exists()),None)
if font_path is None:
 raise RuntimeError('Nenhuma fonte TrueType compativel encontrada (DejaVu Sans Bold, Arial Bold ou Segoe UI Semibold).')
font=ImageFont.truetype(str(font_path),132)
faces=[
((0,0,1),[(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)],'1'),
((1,0,0),[(1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)],'2'),
((0,1,0),[(1,1,1),(-1,1,1),(-1,1,-1),(1,1,-1)],'3'),
((-1,0,0),[(-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)],'4'),
((0,-1,0),[(-1,-1,1),(1,-1,1),(1,-1,-1),(-1,-1,-1)],'5'),
((0,0,-1),[(1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)],'6')]
data=[]
for normal,verts,num in faces:
 im=Image.new('RGBA',(256,256));d=ImageDraw.Draw(im)
 bb=d.textbbox((0,0),num,font=font,stroke_width=9)
 d.text(((256-(bb[2]-bb[0]))/2-bb[0],(256-(bb[3]-bb[1]))/2-bb[1]),num,font=font,fill='#f8edcf',stroke_width=9,stroke_fill='#101116')
 if num=='3': im=im.rotate(180)
 if num=='5': im=im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
 data.append((np.array(normal,float),np.array(verts,float),np.array(im.transpose(Image.Transpose.FLIP_TOP_BOTTOM))))
def rotation(value,t):
 n=data[value-1][0]
 final=Rotation.from_euler('xy',[24,-24],degrees=True)*Rotation.align_vectors([np.array([0,0,1])],[n])[0]
 start=Rotation.from_euler('xyz',[146,84,132],degrees=True)
 return Slerp([0,1],Rotation.concatenate([start,final]))([t*t*(3-2*t)])[0].as_matrix()
def render(value,t):
 R=rotation(value,t); canvas=np.zeros((S,S,3),np.uint8); surfaces=[]
 for normal,verts,tex in data:
  p=verts@R.T;nn=normal@R.T
  if nn[2]<.015:continue
  q=np.column_stack((C+p[:,0]*S*.245,C-p[:,1]*S*.245))
  surfaces.append((p[:,2].mean(),q,nn,tex))
 surfaces.sort(key=lambda x:x[0])
 for _,q,normal,tex in surfaces:
  contour=np.rint(q).astype('int32');mask=np.zeros((S,S),np.uint8)
  cv2.fillConvexPoly(mask,contour,255,cv2.LINE_AA)
  k=.79+.21*max(0,np.dot(normal,np.array([-.45,-.55,.71])))
  canvas[mask>0]=np.clip(np.array([12,85,198],float)*k,0,255).astype('uint8')
  H=cv2.getPerspectiveTransform(np.float32([[0,0],[255,0],[255,255],[0,255]]),q.astype('float32'))
  warped=cv2.warpPerspective(tex,H,(S,S),flags=cv2.INTER_CUBIC,borderMode=cv2.BORDER_CONSTANT)
  alpha=(warped[:,:,3].astype('float32')/255)*(mask.astype('float32')/255)
  canvas=np.clip(canvas.astype('float32')*(1-alpha[:,:,None])+warped[:,:,:3].astype('float32')*alpha[:,:,None],0,255).astype('uint8')
  for color,width in [((15,12,8),max(1,S//100)),((239,174,48),max(1,S//105)),((255,212,107),max(1,S//152))]:
   cv2.polylines(canvas,[contour],True,color,width,cv2.LINE_AA)
 return Image.fromarray(canvas).resize((W,W),Image.Resampling.LANCZOS)
def pack(im):
 a=np.asarray(im,dtype=np.uint16);p=(((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)).flatten()
 b=bytearray();i=0
 while i<len(p):
  v=int(p[i]);run=1
  while run<255 and i+run<len(p) and p[i+run]==v:run+=1
  b.extend((run,v&255,v>>8));i+=run
 return b
streams=[pack(render(v,j/(N-1))) for v in range(1,7) for j in range(N)]
streams.extend(pack(render(v,1)) for v in range(1,7))
offsets=[];lengths=[];stream=bytearray()
for b in streams:offsets.append(len(stream));lengths.append(len(b));stream.extend(b)
def arr(name,values,t):
 return 'static const '+t+' '+name+'[] = {\n'+',\n'.join(','.join(map(str,values[i:i+24])) for i in range(0,len(values),24))+'\n};\n'
h='#ifndef DIAL_D6_H\n#define DIAL_D6_H\n#include <stdint.h>\n#define DIAL_D6_PYTHON_MESH_V1 1\n#define DIAL_D6_COMPACT_V2 1\n#define DIAL_D6_WIDTH 176\n#define DIAL_D6_HEIGHT 176\n#define DIAL_D6_ROLL_COUNT 8\n#define DIAL_D6_TOTAL 54\n'
h+=arr('dial_d6_offsets',offsets,'uint32_t')+arr('dial_d6_lengths',lengths,'uint32_t')+arr('dial_d6_stream',stream,'uint8_t')+'#endif\n'
OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_text(h)
print('Generated',OUT,len(streams),'frames,',len(h),'header chars')

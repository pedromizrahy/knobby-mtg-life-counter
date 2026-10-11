"""Render real polyhedral Blender meshes into firmware RLE/RGB565 sprites.

Usage: blender -b --python tools/dice3d/render_d6.py -- --output knobby/src/dial_d6_frames.h
The licensed mesh generator is vendored under vendor/.
Runs entirely offline once Blender is installed. No 2D cube impostor.
"""
import sys, math, struct, pathlib
import bpy
from mathutils import Vector, Euler, Quaternion
ROOT=pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/"vendor"))
from blender_dice_gen import Cube

W=196
FRAMES_PER_RESULT=12
STEPS=6

def rgb565(r,g,b):
    return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)

def frame_bytes(render_path):
    img=bpy.data.images.load(str(render_path),check_existing=False)
    pixels=list(img.pixels)
    width,height=img.size
    assert (width,height)==(W,W)
    out=bytearray()
    # Blender images are stored bottom-up; firmware needs top-down.
    for y in range(height-1,-1,-1):
        for x in range(width):
            i=4*(y*width+x)
            # PNG loaded as floats (typically decoded sRGB).
            r=int(max(0,min(255,round(pixels[i]*255))))
            g=int(max(0,min(255,round(pixels[i+1]*255))))
            b=int(max(0,min(255,round(pixels[i+2]*255))))
            out+=struct.pack("<H",rgb565(r,g,b))
    bpy.data.images.remove(img)
    return out

def encode_rle(raw):
    values=memoryview(raw)
    encoded=bytearray()
    i=0; count=len(raw)//2
    while i<count:
        first=2*i
        run=1
        while run<255 and i+run<count and values[first:first+2]==values[2*(i+run):2*(i+run)+2]:
            run+=1
        encoded.append(run)
        encoded+=values[first:first+2]
        i+=run
    return encoded

def material(name, color, metallic=0, roughness=0.32):
    m=bpy.data.materials.new(name)
    m.diffuse_color=(*color,1)
    m.use_nodes=True
    p=m.node_tree.nodes.get("Principled BSDF")
    p.inputs["Base Color"].default_value=(*color,1)
    p.inputs["Metallic"].default_value=metallic
    p.inputs["Roughness"].default_value=roughness
    return m

def main():
    argv=sys.argv
    args=argv[argv.index("--")+1:] if "--" in argv else []
    if len(args)!=2 or args[0]!="--output":
        raise SystemExit("usage: blender -b --python render_d6.py -- --output output.h")
    dest=pathlib.Path(args[1]).resolve()
    bpy.ops.object.select_all(action="SELECT");bpy.ops.object.delete(use_global=False)
    scene=bpy.context.scene
    scene.render.engine="CYCLES"
    scene.cycles.samples=32
    scene.render.resolution_x=scene.render.resolution_y=W
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format="PNG"
    scene.render.film_transparent=False
    scene.view_settings.view_transform="Standard"
    scene.view_settings.look="Medium High Contrast"
    scene.world.color=(0,0,0)
    if scene.world.use_nodes:
        bg=scene.world.node_tree.nodes.get("Background")
        if bg: bg.inputs["Color"].default_value=(0,0,0,1)
    root=bpy.data.objects.new("DieRotation",None)
    scene.collection.objects.link(root)
    mesh=Cube("D6",2.4)
    mesh.create(bpy.context)
    die=mesh.dice_mesh
    die.parent=root
    blue=material("Enamel Blue",(.018,.11,.52),.38,.25)
    gold=material("Metal Gold",(.85,.43,.06),.75,.2)
    black=material("Numeral Black Outline",(.002,.006,.02),.1,.4)
    white=material("Numeral Ivory",(.97,.95,.85),.08,.29)
    die.data.materials.clear()
    die.data.materials.append(blue)
    bevel=die.modifiers.new("Gold Rim",type="BEVEL")
    bevel.width=.13;bevel.segments=3
    bevel.affect="EDGES" if hasattr(bevel,"affect") else None
    bevel.loop_slide=True
    bevel.profile=.5
    # Material index: appended after blue for bevel surfaces.
    die.data.materials.append(gold)
    bevel.material=1
    die.modifiers.new("Weighted Face Normals","WEIGHTED_NORMAL")
    font_file=ROOT/"number_font.ttf"
    customfont=bpy.data.fonts.load(str(font_file)) if font_file.exists() else None
    normals=[]
    for val,(loc,rotation) in enumerate(zip(mesh.get_number_locations(),mesh.get_number_rotations()),start=1):
        loc=Vector(loc)
        normal=loc.normalized()
        normals.append(normal)
        for layer,scale,color,offset in [("border",1.18,black,.025),("ink",1.0,white,.045)]:
            curve=bpy.data.curves.new(f"{layer}{val}","FONT")
            curve.body=str(val)
            curve.align_x="CENTER";curve.align_y="CENTER"
            curve.size=.87*scale
            curve.extrude=.004
            curve.bevel_depth=.003
            if customfont:curve.font=customfont
            obj=bpy.data.objects.new(f"{layer}-{val}",curve)
            scene.collection.objects.link(obj)
            obj.location=loc+normal*offset
            obj.rotation_euler=Euler(rotation,"XYZ")
            obj.data.materials.append(color)
            obj.parent=root
    bpy.ops.object.camera_add(location=(5,-7,5))
    cam=bpy.context.object;cam.rotation_euler=(Vector((0,0,0))-cam.location).to_track_quat("-Z","Y").to_euler()
    cam.data.type="ORTHO";cam.data.ortho_scale=4.05;scene.camera=cam
    def light(name,loc,power,size):
        bpy.ops.object.light_add(type="AREA",location=loc)
        ob=bpy.context.object;ob.name=name;ob.data.energy=power
        ob.data.shape="DISK";ob.data.size=size
        ob.rotation_euler=(-ob.location).to_track_quat("-Z","Y").to_euler()
    light("Key",(-3,-4,6),600,5)
    light("Fill",(4,3,4),300,4)
    direction=cam.location.normalized()
    raw_frames=[]
    render_path=dest.parent/"_dice3d_temp.png"
    scene.render.filepath=str(render_path)
    # Each result has its own continuous 3D trajectory. The last render is
    # also used as its static result, so there is no abrupt sprite switch.
    from mathutils import Matrix
    targets=[]
    for normal in normals:
        q=normal.rotation_difference(direction)
        # Tilt the cube to make neighbouring faces visible without
        # obscuring the target face number.
        targets.append(q)
    for value in range(1,7):
        end=targets[value-1]
        # Deterministic real 3D spin; smoothly settles on the selected face.
        start=Quaternion((0.26,0.68,-0.19,0.64)).normalized()
        for f in range(FRAMES_PER_RESULT):
            t=(f+1)/FRAMES_PER_RESULT
            ease=1-(1-t)**3
            root.rotation_mode="QUATERNION"
            q=start.slerp(end,ease)
            spin=Quaternion(direction,math.tau*2*(1-ease))
            root.rotation_quaternion=spin@q
            bpy.ops.render.render(write_still=True)
            raw_frames.append(encode_rle(frame_bytes(render_path)))
    # Last frame of each path is duplicated as static numbered result.
    for value in range(1,7):
        raw_frames.append(raw_frames[value*FRAMES_PER_RESULT-1])
    if render_path.exists():render_path.unlink()
    offsets=[];lengths=[];stream=bytearray()
    for f in raw_frames:
        offsets.append(len(stream));lengths.append(len(f));stream.extend(f)
    def nums(name,vals,t="uint32_t"):
        return f"static const {t} {name}[] = {{\n"+",".join(map(str,vals))+"};\n"
    header=(
        "/* GENERATED by tools/dice3d/render_d6.py — do not hand edit. */\n"
        "#pragma once\n#include <stdint.h>\n"
        f"#define DIAL_D6_WIDTH {W}\n#define DIAL_D6_HEIGHT {W}\n"
        f"#define DIAL_D6_ROLL_COUNT {FRAMES_PER_RESULT}\n"
        f"#define DIAL_D6_TOTAL {len(raw_frames)}\n"
        +nums("dial_d6_offsets",offsets)
        +nums("dial_d6_lengths",lengths)
        +"static const uint8_t dial_d6_stream[] = {\n"
        +",".join(map(str,stream))+"};\n"
    )
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(header,encoding="ascii")
    print(f"D6: {len(raw_frames)} frames; {len(stream)} RLE bytes; saved {dest}")
if __name__=="__main__":main()

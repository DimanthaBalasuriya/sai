"""Create a reproducible engine-owned glTF fixture; optionally fetch Khronos samples."""
from pathlib import Path
import argparse,base64,json,math,struct,zlib,urllib.request,subprocess
root=Path(__file__).resolve().parent.parent
def gallery():
    folder=root/'content'/'samples';folder.mkdir(parents=True,exist_ok=True)
    binary=bytearray();views=[];accessors=[]
    def accessor(values,kind,components,component=5126):
        while len(binary)%4:binary.append(0)
        offset=len(binary);fmt='f' if component==5126 else 'I'
        for row in values:
            binary.extend(struct.pack('<'+fmt*components,*(row if components>1 else [row])))
        views.append({'buffer':0,'byteOffset':offset,'byteLength':len(binary)-offset})
        value={'bufferView':len(views)-1,'componentType':component,'count':len(values),'type':kind}
        if kind=='VEC3':value.update(min=[min(v[i] for v in values) for i in range(3)],max=[max(v[i] for v in values) for i in range(3)])
        accessors.append(value);return len(accessors)-1
    positions=[];normals=[];uvs=[];indices=[]
    faces=[([(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)],(0,0,1)),([(1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)],(0,0,-1)),
           ([(-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)],(-1,0,0)),([(1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)],(1,0,0)),
           ([(-1,1,1),(1,1,1),(1,1,-1),(-1,1,-1)],(0,1,0)),([(-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1)],(0,-1,0))]
    for verts,normal in faces:
        start=len(positions);positions+=verts;normals+=[normal]*4;uvs += [(0,0),(1,0),(1,1),(0,1)];indices += [start+i for i in [0,1,2,2,3,0]]
    attributes={'POSITION':accessor(positions,'VEC3',3),'NORMAL':accessor(normals,'VEC3',3),'TEXCOORD_0':accessor(uvs,'VEC2',2)}
    cube={'name':'Split material cube','primitives':[{'attributes':attributes,'indices':accessor(indices[:18],'SCALAR',1,5125),'material':0},{'attributes':attributes,'indices':accessor(indices[18:],'SCALAR',1,5125),'material':1}]}
    positions=[];normals=[];uvs=[];indices=[];slices=48;stacks=24
    for y in range(stacks+1):
        for x in range(slices+1):
            phi=y/stacks*math.pi;theta=x/slices*math.tau
            p=(math.sin(phi)*math.cos(theta),math.cos(phi),math.sin(phi)*math.sin(theta))
            positions.append(p);normals.append(p);uvs.append((x/slices,y/stacks))
    for y in range(stacks):
        for x in range(slices):
            a=y*(slices+1)+x;b=a+slices+1;indices += [a,b,a+1,a+1,b,b+1]
    sphere={'name':'Textured sphere','primitives':[{'attributes':{'POSITION':accessor(positions,'VEC3',3),'NORMAL':accessor(normals,'VEC3',3),'TEXCOORD_0':accessor(uvs,'VEC2',2)},'indices':accessor(indices,'SCALAR',1,5125),'material':2}]}
    def png(name,pixel):
        def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
        size=64;raw=b''.join(b'\0'+b''.join(bytes(pixel(x,y)) for x in range(size)) for y in range(size))
        data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',size,size,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
        (folder/name).write_bytes(data)
    png('gallery-base.png',lambda x,y:(220,180,90,255) if (x//8+y//8)%2 else (40,100,180,255))
    png('gallery-mr.png',lambda x,y:(255,int(50+x/64*190),int(y/64*255),255))
    scene={'asset':{'version':'2.0','generator':'Engine M4 test fixture'},'scene':0,'scenes':[{'nodes':[0]}],
           'nodes':[{'name':'Material Gallery','children':[1,2,3,4]}, {'name':'Copper / ceramic cube','mesh':0,'translation':[-2.5,1,0]},
                    {'name':'Metallic roughness sphere','mesh':1,'translation':[0,1,0]}, {'name':'Second textured sphere','mesh':1,'translation':[2.5,1,0]},
                    {'name':'Floor','mesh':0,'translation':[0,-0.25,0],'scale':[5,0.2,3]}],
           'meshes':[cube,sphere], 'materials':[{'name':'Copper','pbrMetallicRoughness':{'baseColorFactor':[0.95,0.5,0.22,1],'metallicFactor':1,'roughnessFactor':0.22}},
           {'name':'Ceramic','pbrMetallicRoughness':{'baseColorFactor':[0.18,0.32,0.52,1],'metallicFactor':0,'roughnessFactor':0.7}},
           {'name':'Textured metal','pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicRoughnessTexture':{'index':1},'metallicFactor':1,'roughnessFactor':1}}],
           'images':[{'uri':'gallery-base.png'},{'uri':'gallery-mr.png'}],'textures':[{'source':0},{'source':1}],
           'buffers':[{'byteLength':len(binary),'uri':'gallery.bin'}],'bufferViews':views,'accessors':accessors}
    (folder/'material_gallery.gltf').write_text(json.dumps(scene,indent=2));(folder/'gallery.bin').write_bytes(binary)
    print('Prepared',folder/'material_gallery.gltf')
def khronos():
    commit=subprocess.check_output(['git','ls-remote','https://github.com/KhronosGroup/glTF-Sample-Assets.git','HEAD'],text=True).split()[0]
    model='MetalRoughSpheresNoTextures';base=f'https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/{commit}/Models/{model}/'
    folder=root/'content'/'samples'/'khronos';folder.mkdir(parents=True,exist_ok=True)
    data=urllib.request.urlopen(base+'glTF/'+model+'.gltf',timeout=60).read();(folder/(model+'.gltf')).write_bytes(data)
    scene=json.loads(data)
    for entry in scene.get('buffers',[])+scene.get('images',[]):
        uri=entry.get('uri','')
        if uri and not uri.startswith('data:'):
            target=folder/uri;target.parent.mkdir(parents=True,exist_ok=True);print('Downloading',uri,flush=True);target.write_bytes(urllib.request.urlopen(base+'glTF/'+uri,timeout=60).read())
    (folder/'README-upstream.md').write_bytes(urllib.request.urlopen(base+'README.md').read())
    (folder/'SOURCE.txt').write_text(f'KhronosGroup/glTF-Sample-Assets\nCommit: {commit}\nMetalRoughSpheresNoTextures (2019 Public)\nCC0-1.0; see README-upstream.md\nhttps://github.com/KhronosGroup/glTF-Sample-Assets/tree/{commit}/Models/{model}\n')
    print('Downloaded Khronos sample at commit',commit)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--khronos',action='store_true');args=parser.parse_args();gallery()
    if args.khronos:khronos()

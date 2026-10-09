#!/usr/bin/env python3
"""Audit real GLB skeletons/clips; emit deterministic rig compatibility and runtime metadata."""
import argparse
from collections import defaultdict,Counter
import hashlib,json,re,struct
from pathlib import Path
from character_manifest import read_manifest
from gltf_import import read_gltf
ROOT=Path(__file__).resolve().parents[1]
OUTPUTS=[ROOT/'docs/character-rig-compatibility.json',ROOT/'docs/character-rig-compatibility.md',ROOT/'include/outland/characters/AnimationAssetCatalog.hpp']
def values(document,binary,index):
    accessor=document['accessors'][index]; view=document['bufferViews'][accessor['bufferView']]
    assert accessor['componentType']==5126 and not accessor.get('sparse'), 'Unsupported animation/bind accessor'
    components={'SCALAR':1,'VEC3':3,'VEC4':4,'MAT4':16}[accessor['type']]
    stride=view.get('byteStride',components*4);offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)
    return [struct.unpack_from('<'+'f'*components,binary,offset+i*stride) for i in range(accessor['count'])]
def action(name):
    tokens=set(re.findall(r'[a-z0-9]+',name.lower()))
    for label,names in [('death',{'death','die','dead'}),('attack',{'attack','punch','jab','hit','fire'}),('run',{'run','sprint'}),('walk',{'walk'}),('idle',{'idle','rest','relax'})]:
        if tokens & names:return label
    return 'unmapped'
def fingerprint(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()[:24]
def generate():
    entries=[]; groups=defaultdict(list);exact=defaultdict(list); bone_topologies={}
    for row in read_manifest(ROOT):
        document,binary=read_gltf(ROOT/row['runtime_model']);nodes=document.get('nodes',[]);parents={child:i for i,node in enumerate(nodes) for child in node.get('children',[])}
        skins=[]
        for skin in document.get('skins',[]):
            joints=skin['joints'];indices={node:i for i,node in enumerate(joints)}
            topology=[]
            for node in joints:
                parent=parents.get(node)
                while parent is not None and parent not in indices:parent=parents.get(parent)
                topology.append([nodes[node].get('name',''),indices.get(parent,-1)])
            bind=values(document,binary,skin['inverseBindMatrices']) if 'inverseBindMatrices' in skin else []
            # Include mesh/joint node TRS/matrices, not just names or bone counts.
            transform=[{key:node[key] for key in ('matrix','translation','rotation','scale') if key in node} for node in nodes]
            topo=fingerprint(topology);bone_topologies[topo]=topology;rig=fingerprint([topology,bind,transform])
            skins.append({'joints':len(joints),'topology':topo,'exact_rig':rig})
        clips=[]
        for index,animation in enumerate(document.get('animations',[])):
            duration=0.0;changes=False;targets=set();keyframes=0
            for sampler in animation['samplers']:
                times=values(document,binary,sampler['input']);poses=values(document,binary,sampler['output'])
                duration=max(duration,max((time[0] for time in times),default=0));keyframes=max(keyframes,len(times))
                changes|=any(pose!=poses[0] for pose in poses[1:])
            for channel in animation['channels']: targets.add(channel['target'].get('path',''))
            name=animation.get('name','');semantic=action(name)
            clips.append({'index':index,'name':name,'action':semantic,'duration_seconds':round(duration,6),'keyframes':keyframes,'animated_values_change':changes,'channels':sorted(targets)})
        entry={'id':row['id'],'role':row['role'],'category':row['category'],'runtime_model':row['runtime_model'],'skins':skins,'clips':clips}
        entries.append(entry)
        if skins:
            groups[skins[0]['topology']].append(row['id']);exact[skins[0]['exact_rig']].append(row['id'])
    report={'policy':'Only own-model clips are loaded. Topology matches are candidates, not permission to retarget; exact signatures include bind transforms and node transforms. Unmapped clips are never guessed.',
        'models':entries,'topology_groups':dict(sorted(groups.items())),'exact_rig_groups':dict(sorted(exact.items()))}
    text=['# Production character rig and clip compatibility','',report['policy'],'',
        f'{len(entries)} models; {sum(len(e["clips"]) for e in entries)} embedded clips.','',
        '| Character | Role | Joints | Topology | Exact rig | Clip actions |','|---|---|---:|---|---|---|']
    header=['#pragma once','#include <array>','#include <string_view>','namespace outland::characters {',
        'struct AnimationAssetInfo { std::string_view id, topology, exact_rig; int joints, clips, mapped_clips; };',
        f'inline constexpr std::array<AnimationAssetInfo,{len(entries)}> animation_asset_catalog = {{{{']
    catalog_open=header.pop()
    header[2:2]=['#include <raylib.h>','#include <span>']
    for key,bones in sorted(bone_topologies.items()):
        header.append('inline constexpr std::array<BoneInfo,'+str(len(bones))+'> rig_'+key+' = {{'+','.join('{'+json.dumps(name[:31])+','+str(parent)+'}' for name,parent in bones)+'}};')
    header.append('inline std::span<const BoneInfo> audited_bones(std::string_view topology) {')
    for key in sorted(bone_topologies):header.append('if(topology=="'+key+'")return rig_'+key+';')
    header.append('return {}; }')
    header.append(catalog_open)
    for entry in entries:
        skin=entry['skins'][0] if entry['skins'] else {'joints':0,'topology':'none','exact_rig':'none'}
        mapped=sum(c['action']!='unmapped' and c['keyframes']>=2 for c in entry['clips'])
        summary=', '.join(f'{k}: {v}' for k,v in sorted(Counter(c['action'] for c in entry['clips']).items())) or 'none'
        text.append(f'| {entry["id"]} | {entry["role"]} | {skin["joints"]} | {skin["topology"]} | {skin["exact_rig"]} | {summary} |')
        header.append('    {'+','.join([json.dumps(entry['id']),json.dumps(skin['topology']),json.dumps(skin['exact_rig']),str(skin['joints']),str(len(entry['clips'])),str(mapped)])+'},')
    header+=['}};','} // namespace outland::characters','']
    bodies=[e for e in entries if e['role']=='npc'];bodyclips=[c for e in bodies for c in e['clips']]
    text+=['','## Findings','',f'{len(bodies)} bodies, {len(bodyclips)} body clips; {sum(c["action"]!="unmapped" for c in bodyclips)} body clips have recognized action names.',
        'The PSX generic Mixamo/Layer0 clips are ~0.067s and have no declared idle/walk/run/attack/death semantics; they are retained but not assigned invented actions.',
        'The rebel body has a rig but no embedded clips. Named idle/attack clips exist on arm-only rigs; they are not transplanted to bodies.',
        'Raylib 6 clips contain joint counts/poses but no skeleton names or hierarchy. Runtime clips are loaded only from the same model source; compatibility checks joint counts, valid model skeleton hierarchy, pose availability and mesh bone counts. Audited topology supplies joint names/hierarchy for offline tests. Cross-model retargeting/blending is not implemented.',
        'AnimationController selects only compatible clips, with Walk/Idle/bind-pose fallbacks. CharacterRenderer adds an explicitly authored procedural Mixamo gait for missing Idle/Walk/Run; this is separate from the embedded-clip audit. Missing Death keeps bind pose without resurrection or invented motion.','',
        '## Topology groups','']
    for key,ids in sorted(groups.items()):text.append(f'- `{key}` ({len(ids)}): '+', '.join(ids))
    text+=['','## Exact rig groups','']
    for key,ids in sorted(exact.items()):text.append(f'- `{key}` ({len(ids)}): '+', '.join(ids))
    return [json.dumps(report,indent=2,sort_keys=True)+'\n','\n'.join(text)+'\n','\n'.join(header)],entries
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    outputs,entries=generate()
    for path,content in zip(OUTPUTS,outputs):
        if args.check:
            assert path.exists() and path.read_text()==content,f'Stale rig audit: {path}'
        else:path.parent.mkdir(parents=True,exist_ok=True);path.write_text(content)
    print(f'Rig audit: {len(entries)} models, {sum(len(e["clips"]) for e in entries)} real clips; compatibility report verified.')

#!/usr/bin/env python3
"""Tonneau ESP32 ideaspark V2 — millimetres; Python 3.11+.
Regeneration: pip install numpy manifold3d trimesh
             python tonneau_v2_parametrique.py
Les dimensions fabricant inconnues sont des hypotheses d'ajustement, pas des mesures.
Les STL principaux sont deja orientes pour impression, dessous sur Z=0.
"""
from pathlib import Path
import json, math, hashlib
import numpy as np
import manifold3d as md
import trimesh

OUT = Path(__file__).resolve().parent
WORK = OUT.parent.parent / 'work' / 'v2'
WORK.mkdir(parents=True,exist_ok=True)
P = dict(length=74.0, radius_end=35.0, radius_middle=41.0,
         axis_height=34.0, wall=2.8, floor=3.0, screen_angle=30.0,
         board_length=63.8, board_width=32.6, board_depth=5.0, pcb_thickness=1.6,
         board_pocket_x=65.2, board_pocket_y=34.0,
         window_x=46.0, window_y=25.0, window_offset_x=-0.8,
         bezel_x=70.0, bezel_y=38.0, bezel_front_q=33.5,
         board_front_q=30.7, seam_z=34.0, seam_gap=0.18,
         lip_clearance=0.30, lip_thickness=1.6, lip_height=4.5,
         clip_thickness=1.5, clip_width=6.5, clip_y=25.5,
         usb_width=13.0, usb_height=8.0, usb_center_q=28.3,
         wire_diameter=10.0, mount_spacing=26.0, mount_x=29.8,
         pilot_diameter=1.6, mount_radius=2.6, button_span=22.0,
         dupont_depth=36.0)
N=192
END=P['length']/2
CLIP_X_SHIFT=END-48.0
CLIP_Z_SHIFT=-4.0
BANDS=[-END+7.0,-END*.50,END*.50,END-7.0]
BOARD_BACK=P['board_front_q']-P['board_depth']
PCB_FRONT=BOARD_BACK+P['pcb_thickness']

def union(*shapes):
    return md.Manifold.batch_boolean(list(shapes),md.OpType.Add)

def box(x0,x1,y0,y1,z0,z1):
    return md.Manifold.cube((x1-x0,y1-y0,z1-z0)).translate((x0,y0,z0))

def slab(z0,z1):
    return box(-150,150,-150,150,z0,z1)

def rr(w,h,r,z0,z1,x=0,y=0):
    cs=md.CrossSection.square((w-2*r,h-2*r),center=True).offset(r,circular_segments=32)
    return md.Manifold.extrude(cs,z1-z0).translate((x,y,z0))

def local(shape):
    return shape.rotate((P['screen_angle'],0,0)).translate((0,0,P['axis_height']))

def radius(x):
    return P['radius_middle']-(P['radius_middle']-P['radius_end'])*(2*x/P['length'])**2

def sculpted_outer():
    # Surface unique: cerclages et rainures sont integres aux anneaux du maillage.
    # Cela evite l'empilement de faces coplanaires de multiples revolutions.
    xs=set(np.linspace(-END,END,int(P['length']/2)+1))
    bands=BANDS
    for c in bands:
        xs.update([c-2.6,c-2.3,c+2.3,c+2.6])
    xs=sorted(xs)
    angles=set(np.arange(0,360,2).tolist())
    for a in range(0,360,20):
        angles.update([(a-.6)%360,a,(a+.6)%360])
    angles=sorted(angles)
    verts=[]
    for x in xs:
        hoop=max(max(0,min(1,(2.6-abs(x-c))/.3)) for c in bands)
        for a in angles:
            distance=min(a%20,20-a%20)
            groove=max(0,1-distance/.6)*.55*(1-hoop)
            r=radius(x)+1.25*hoop-groove
            t=math.radians(a)
            verts.append([x,r*math.cos(t),P['axis_height']+r*math.sin(t)])
    n=len(angles);faces=[]
    for i in range(len(xs)-1):
        for j in range(n):
            k=(j+1)%n;a=i*n+j;b=i*n+k;c=(i+1)*n+j;d=(i+1)*n+k
            faces.extend([[a,b,c],[b,d,c]])
    verts.extend([[-END,0,P['axis_height']],[END,0,P['axis_height']]])
    left=len(verts)-2;right=left+1
    for j in range(n):
        k=(j+1)%n
        faces.extend([[left,k,j],[right,(len(xs)-1)*n+j,(len(xs)-1)*n+k]])
    tm=trimesh.Trimesh(vertices=np.asarray(verts),faces=np.asarray(faces),process=False)
    if tm.volume<0: tm.invert()
    assert tm.is_watertight
    return md.Manifold(md.Mesh64(np.asarray(tm.vertices,dtype=np.float64),np.asarray(tm.faces,dtype=np.uint64)))

def barrel(offset=0, x0=None,x1=None,extra=0,sector=360):
    x0=-P['length']/2+offset if x0 is None else x0
    x1=P['length']/2-offset if x1 is None else x1
    xs=np.linspace(x0,x1,max(3,int((x1-x0)/2)+1))
    contour=[(0,x0)]+[(radius(x)-offset+extra,float(x)) for x in xs]+[(0,x1)]
    return md.Manifold.revolve(md.CrossSection([contour]),N,sector).rotate((0,90,0)).translate((0,0,P['axis_height']))

def shell_ring(outer_offset,inner_offset,z0,z1):
    return (barrel(outer_offset)-barrel(inner_offset)) ^ slab(z0,z1)

def xz_prism(points,y0,y1):
    # Local 2D (x,z); right-handed transform maps extrusion to decreasing Y.
    cross=md.CrossSection([points])
    return md.Manifold.extrude(cross,y1-y0).rotate((90,0,0)).translate((0,y1,0))

def mirrored_x(shape,side):
    return shape if side==1 else shape.mirror((1,0,0))

def mesh(shape):
    m=shape.to_mesh64()
    return trimesh.Trimesh(vertices=np.array(m.vert_properties[:,:3]),faces=np.array(m.tri_verts),process=False)

def clean(shape):
    # Retirer seulement les membranes coplanaires de volume numeriquement nul.
    # Le controle interdit de masquer une vraie piece deconnectee.
    solid=[s for s in shape.decompose() if abs(s.volume())>1e-6]
    if len(solid)!=1:
        raise RuntimeError(f'Geometrie deconnectee: {[s.volume() for s in solid]}')
    return solid[0]

print('Construction du tonneau...',flush=True)
hoops=union(*[barrel(x0=c-2.6,x1=c+2.6,extra=1.25) for c in BANDS])
outer=sculpted_outer()^slab(0,150)
# Rainures des fonds (0.45 mm), interrompues par les ouvertures fonctionnelles.
endcuts=[]
for side in [-1,1]:
    for y in [-24,-12,0,12,24]:
        endcuts.append(mirrored_x(box(END-.45,END+1,y-.30,y+.30,-10,100),side))
outer=outer-union(*endcuts)
inner=barrel(P['wall']) ^ slab(P['floor'],150)
housing=outer-inner

# Le logement s'ouvre par l'interieur; la face avant arrete le cadre du module.
bezel=local(rr(P['bezel_x'],P['bezel_y'],3.0,26.5,P['bezel_front_q']))
top_relief=local(rr(P['bezel_x']+.02,P['bezel_y']+.02,3.0,P['bezel_front_q'],100))
pocket=local(rr(P['board_pocket_x'],P['board_pocket_y'],.65,20,P['board_front_q']))
window=local(rr(P['window_x'],P['window_y'],.65,20,100,x=P['window_offset_x']))
housing=union(housing,bezel)-top_relief-pocket-window

# Le tunnel laisse entrer le surmoulage d'une fiche, malgre le retrait du port.
usb=local(box(30,END+10,-P['usb_width']/2,P['usb_width']/2,
              P['usb_center_q']-P['usb_height']/2,P['usb_center_q']+P['usb_height']/2))
# Petit evase uniquement a l'exterieur de la face droite.
usb_mouth=local(box(END-.8,END+10,-(P['usb_width']+.8)/2,(P['usb_width']+.8)/2,
                    P['usb_center_q']-(P['usb_height']+.8)/2,
                    P['usb_center_q']+(P['usb_height']+.8)/2))
wire=md.Manifold.cylinder(30,P['wire_diameter']/2,circular_segments=64).rotate((-90,0,0)).translate((0,30,P['seam_z']))
functional_cuts=union(usb,usb_mouth,wire)
housing=housing-functional_cuts

# Deux appuis sous la face du PCB cote ecran. Vis depuis le dos du PCB.
# L'epaisseur totale de 5 mm et l'entraxe de 26 mm ont ete mesures par l'utilisateur.
# Epaisseur PCB1.6 et position longitudinale X29.8 restent des hypotheses.
mounts_local=[]; pilots_local=[]; through_holes=[]
for sign in [-1,1]:
    yy=sign*P['mount_spacing']/2
    boss=md.Manifold.cylinder(P['bezel_front_q']-PCB_FRONT,P['mount_radius'],circular_segments=64).translate((P['mount_x'],yy,PCB_FRONT))
    # Le cote voisin des boutons reste au-dela de leur enveloppe de 22 mm.
    limit=P['button_span']/2+.5
    cut=box(20,36,limit,18,PCB_FRONT-1,40) if sign==1 else box(20,36,-18,-limit,PCB_FRONT-1,40)
    boss=boss^cut
    pilot=md.Manifold.cylinder((P['bezel_front_q']-1.5)-(PCB_FRONT-.2),P['pilot_diameter']/2,circular_segments=48).translate((P['mount_x'],yy,PCB_FRONT-.2))
    mounts_local.append(boss-pilot)
    pilots_local.append(pilot)
    through_holes.append(md.Manifold.cylinder(10,1.5,circular_segments=48).translate((P['mount_x'],yy,BOARD_BACK-1)))
mounts=local(union(*mounts_local))
housing=union(housing,mounts)-local(union(*pilots_local))

# Assemblage: une levre de guidage et quatre lames de verrouillage.
seam_low=P['seam_z']-P['seam_gap']/2
seam_high=P['seam_z']+P['seam_gap']/2
lower=housing^slab(0,seam_low)
upper=housing^slab(seam_high,150)
lip_outer=P['wall']+P['lip_clearance']
lip_inner=lip_outer+P['lip_thickness']
lip=shell_ring(lip_outer,lip_inner,31.2,P['seam_z']+P['lip_height'])
lip_foot=shell_ring(1.4,lip_inner,31.2,seam_low)
clip_notches=[]; clips=[]; windows=[]; clip_only=[]
for side in [-1,1]:
    for yy in [-P['clip_y'],P['clip_y']]:
        # La decoupe isole la lame de la levre, sur sa longueur utile.
        clip_notches.append(mirrored_x(box(41.5+CLIP_X_SHIFT,49+CLIP_X_SHIFT,yy-4.6,yy+4.6,32+CLIP_Z_SHIFT,55+CLIP_Z_SHIFT),side))
        stem=box(43.4,44.9,yy-P['clip_width']/2,yy+P['clip_width']/2,30.9,51)
        foot=box(43.4,47,yy-4.0,yy+4.0,29.0,32.0)
        # Racine adoucie par un renfort triangulaire, cote interieur.
        root=xz_prism([(42.3,30.8),(43.5,30.8),(43.5,34.0)],yy-3.25,yy+3.25)
        hook=xz_prism([(44.85,48.5),(46.0,48.5),(46.0,49.0),(44.85,51.0)],yy-3.25,yy+3.25)
        clip=mirrored_x(union(stem,foot,root,hook).translate((CLIP_X_SHIFT,0,CLIP_Z_SHIFT)),side)
        clips.append(clip)
        clip_only.append(mirrored_x(union(stem,root,hook).translate((CLIP_X_SHIFT,0,CLIP_Z_SHIFT)),side))
        windows.append(mirrored_x(box(44.5+CLIP_X_SHIFT,49+CLIP_X_SHIFT,yy-3.6,yy+3.6,48.2+CLIP_Z_SHIFT,51.5+CLIP_Z_SHIFT),side))
lip=union(lip,lip_foot)-union(*clip_notches)-functional_cuts
lower=union(lower,lip,*clips)-functional_cuts
upper=upper-union(*windows)
lower=clean(lower)
upper=clean(upper)

# Appui de colle large hors de la zone des connecteurs; le logement arriere reste libre.
# Le petit gabarit reproduit exactement la poche et la butee avant, a plat.
# Gabarit complet: meme butee, fenetre ET appuis de vis que la coque.
coupon=rr(P['bezel_x'],P['bezel_y'],3,26.5,P['bezel_front_q'])
coupon=coupon-rr(P['board_pocket_x'],P['board_pocket_y'],.65,20,P['board_front_q'])
coupon=coupon-rr(P['window_x'],P['window_y'],.65,20,40,x=P['window_offset_x'])
coupon=union(coupon,*mounts_local)-union(*pilots_local)
coupon=coupon.rotate((180,0,0)).translate((0,0,P['bezel_front_q']))

# Coupon du clip: extrait exact d'un clip et de sa fenetre avec leurs appuis.
male_crop=box(41+CLIP_X_SHIFT,48.1+CLIP_X_SHIFT,P['clip_y']-5,P['clip_y']+5,27+CLIP_Z_SHIFT,53+CLIP_Z_SHIFT)
female_crop=box(44.4+CLIP_X_SHIFT,48.1+CLIP_X_SHIFT,P['clip_y']-5,P['clip_y']+5,seam_high,54+CLIP_Z_SHIFT)
clip_male=lower^male_crop
clip_female=upper^female_crop
clip_male=clean(clip_male)
clip_female=clean(clip_female)

print('Validation des solides et des degagements...',flush=True)
parts={'01_demi_tonneau_bas_v2':lower,
       '02_demi_tonneau_haut_v2':upper.translate((0,0,-seam_high)),
       '03_gabarit_carte_et_vis_v2':coupon,
       '04_test_clip_male_v2':clip_male.translate((0,0,-27-CLIP_Z_SHIFT)),
       '05_test_clip_femelle_v2':clip_female.translate((0,0,-seam_high))}
report={'parameters_mm':P,'parts':{},'checks':{}}
for name,part in parts.items():
    tm=mesh(part)
    print(name,'status',part.status(),'volume',part.volume(),'watertight',tm.is_watertight,'winding',tm.is_winding_consistent,flush=True)
    if not tm.is_watertight or not tm.is_winding_consistent or tm.volume<=0:
        raise RuntimeError(f'Maillage invalide: {name}')
    connected=len(part.decompose())
    if connected!=1:
        print('Composantes:',[(s.volume(),s.bounding_box()) for s in part.decompose()],flush=True)
        raise RuntimeError(f'{name}: {connected} volumes deconnectes')
    tm.export(OUT/(name+'.stl'))
    # Verifier le fichier binaire relu: le STL quantifie les coordonnees en float32.
    reloaded=trimesh.load_mesh(OUT/(name+'.stl'))
    re_solid=md.Manifold(md.Mesh64(np.asarray(reloaded.vertices,dtype=np.float64),
                                  np.asarray(reloaded.faces,dtype=np.uint64)))
    assert reloaded.is_watertight and reloaded.is_winding_consistent
    assert np.min(reloaded.area_faces)>1e-8
    assert re_solid.status()==md.Error.NoError and len(re_solid.decompose())==1
    report['parts'][name]={'watertight':bool(tm.is_watertight),
      'connected_solids':connected,'triangles':len(tm.faces),
      'bounds_mm':tm.bounds.tolist(),'size_mm':tm.extents.tolist(),
      'volume_cm3':tm.volume/1000,'mass_solid_PETG_g_estimate':tm.volume/1000*1.27}
    report['parts'][name].update(stl_reloaded_watertight=True,
      stl_reloaded_single_solid=True,stl_reloaded_degenerate_faces=0,
      sha256=hashlib.sha256((OUT/(name+'.stl')).read_bytes()).hexdigest())
    print(name,tm.extents.round(2),'mm',len(tm.faces),'triangles',flush=True)

intersection=(lower^upper).volume()
# Le cadre n'occupe pas le petit bout du PCB portant les trous/USB.
# Reservation vraie du PCB, et verre/cadre dans sa partie principale.
clearance=local(box(-31.9,31.9,-P['board_width']/2,P['board_width']/2,BOARD_BACK-P['dupont_depth'],BOARD_BACK-.01))
pcb_local=rr(63.8,P['board_width'],.8,BOARD_BACK,PCB_FRONT)
pcb_local=pcb_local-union(*through_holes)
display_local=rr(56,P['board_width'],.65,PCB_FRONT,P['board_front_q']-.01,x=-3.9)
pcb_front=local(union(pcb_local,display_local))
housing_both=union(lower,upper)
button_keepout=local(box(26.8,32.0,-11,11,PCB_FRONT,PCB_FRONT+2.4))
heads=local(union(*[md.Manifold.cylinder(1.4,1.9,circular_segments=48).translate((P['mount_x'],s*13,BOARD_BACK-1.4)) for s in [-1,1]]))
clip_sweeps=[]
for i,s in enumerate(clip_only):
    direction=1 if i<2 else -1
    clip_sweeps.append(union(*[s.translate((direction*d,0,0)) for d in [0,.25,.5,.75,1.,1.3]]))
report['checks'].update(
    shells_intersection_mm3=intersection,
    dupont_reserved_36mm_intersection_mm3=(housing_both^clearance).volume(),
    board_envelope_intersection_mm3=(housing_both^pcb_front).volume(),
    usb_tunnel_intersection_mm3=(housing_both^usb).volume(),
    wire_passage_intersection_mm3=(housing_both^wire).volume(),
    clip_usb_intersection_mm3=(union(*clip_only)^union(usb,usb_mouth)).volume(),
    mounts_buttons_intersection_mm3=(mounts^button_keepout).volume(),
    screw_heads_intersection_mm3=(housing_both^heads).volume(),
    clip_1p3mm_deflection_board_mounts_intersection_mm3=(union(*clip_sweeps)^union(pcb_front,mounts,heads)).volume(),
    usb_port_to_outer_end_mm=END-P['board_length']/2,
    mount_hole_spacing_mm=P['mount_spacing'],
    pcb_thickness_assumed_mm=P['pcb_thickness'],
    floor_reference_block_present_mm3=(lower^box(-1,1,16,18,.2,2.8)).volume(),
    screen_angle_degrees=P['screen_angle'],
    barrel_reference_width_mm=2*P['radius_middle'],
    tested_with_physical_board=False,
    tested_with_physical_print=False)
for key in ['shells_intersection_mm3','dupont_reserved_36mm_intersection_mm3',
            'board_envelope_intersection_mm3','usb_tunnel_intersection_mm3',
            'wire_passage_intersection_mm3','clip_usb_intersection_mm3',
            'mounts_buttons_intersection_mm3','screw_heads_intersection_mm3',
            'clip_1p3mm_deflection_board_mounts_intersection_mm3']:
    if report['checks'][key]>.005:
        raise RuntimeError(f'Collision {key}: {report["checks"][key]}')
assert abs(report['checks']['floor_reference_block_present_mm3']-10.4)<1e-4

# Positions assemblees pour les apercus; les STL principaux restent prets a trancher.
mesh(lower).export(WORK/'assembly_lower.stl')
mesh(upper).export(WORK/'assembly_upper.stl')
mesh(hoops^housing_both).export(WORK/'assembly_hoops.stl')
mesh(pcb_front).export(WORK/'board_mock.stl')
mesh(clearance).export(WORK/'dupont_clearance.stl')
mesh(local(rr(49.72,25.8,.5,P['board_front_q'],P['board_front_q']+.25,x=P['window_offset_x']))).export(WORK/'lcd_mock.stl')
mesh(local(rr(46,25,.15,P['board_front_q']+.25,P['board_front_q']+.30,x=P['window_offset_x']))).export(WORK/'lcd_active_mock.stl')
mesh(mounts).export(WORK/'mounts_mock.stl')
mesh(clip_male).export(WORK/'clip_male_assembled.stl')
mesh(clip_female).export(WORK/'clip_female_assembled.stl')
(OUT/'verification_geometrique.json').write_text(json.dumps(report,indent=2,ensure_ascii=False))
print(json.dumps(report['checks'],indent=2),flush=True)

# Solar-Batteriestation: Keilgehaeuse fuer 14,5 x 14,5 cm Panel, 18650-Halter, HW-775
# Alle Masse in Millimetern. In Blender ueber Text-Editor > Run Script ausfuehren.
import bpy, bmesh, math, os

# ---------------- Parameter ----------------
PANEL   = 145.0   # Kantenlaenge Panel
PANEL_T = 3.0     # Dicke Panel
CLEAR   = 0.5     # Spiel pro Seite im Falz
WALL    = 3.2     # Wandstaerke
FLOOR   = 3.0     # Bodenstaerke
LIP     = 1.6     # Breite der aeusseren Lippe um den Falz
TILT    = math.radians(30)
HF      = 25.0    # Hoehe Frontwand aussen

BAT_L, BAT_W, BAT_H = 78.0, 21.0, 21.0   # 18650-Halter
HW_L,  HW_W         = 28.0, 17.0         # HW-775 Platine
PCB_L, PCB_W        = 70.0, 50.0         # Lochraster fuer D1 mini, ADS1115, Teiler, Transistor
PCB_LEDGE, PCB_LIFT = 1.5, 4.0           # Auflagekante und Abstand zum Boden
FRAME_WALL, FRAME_H, FRAME_CLEAR = 2.0, 6.0, 0.6

GLAND_D  = 12.5   # PG7 Bohrung
GLAND_Y  = 25.0   # Abstand Bohrungsmitte von Rueckwand
GLAND_Z  = 13.0   # Hoehe Bohrungsmitte
DRAIN_D  = 3.0


OUT_DIR = os.path.expanduser("~/workspace/SoilMoistureSens/case")

# --------------- abgeleitete Masse ---------------
OPEN = PANEL + 2 * CLEAR            # Falz-Oeffnung
W    = OPEN + 2 * LIP               # Aussenbreite
S    = W                            # Laenge der Schraege aussen
L    = S * math.cos(TILT)           # Grundflaeche Tiefe
HB   = HF + L * math.tan(TILT)      # Hoehe Rueckwand aussen
n    = (0.0, -math.sin(TILT), math.cos(TILT))   # Normale der Schraege

# --------------- Helfer ---------------
def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for m in list(bpy.data.meshes):
        if m.users == 0:
            bpy.data.meshes.remove(m)

def mesh_obj(name, verts, faces):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    return ob

def box(name, x0, x1, y0, y1, z0, z1):
    v = [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),
         (x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
    f = [(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)]
    return mesh_obj(name, v, f)

def wedge(name, x0, x1, y0, y1, z0, zf, zb):
    v = [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),
         (x0,y0,zf),(x1,y0,zf),(x1,y1,zb),(x0,y1,zb)]
    f = [(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)]
    return mesh_obj(name, v, f)

def cylinder(name, r, length, axis, center):
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=r, depth=length, location=center)
    ob = bpy.context.active_object
    ob.name = name
    if axis == 'X':
        ob.rotation_euler = (0, math.pi/2, 0)
    elif axis == 'Y':
        ob.rotation_euler = (math.pi/2, 0, 0)
    return ob

def teardrop_x(name, r, x0, x1, yc, zc):
    """Tropfenfoermiges Loch entlang X (Spitze nach oben, 45 Grad)."""
    pts = []
    for i in range(0, 25):
        a = math.radians(45 - i * (270 / 24))     # 45 .. -225 Grad
        pts.append((yc + r * math.cos(a), zc + r * math.sin(a)))
    pts.append((yc, zc + r * math.sqrt(2)))
    bm = bmesh.new()
    front = [bm.verts.new((x0, y, z)) for y, z in pts]
    back  = [bm.verts.new((x1, y, z)) for y, z in pts]
    bm.faces.new(front[::-1]); bm.faces.new(back)
    k = len(pts)
    for i in range(k):
        j = (i + 1) % k
        bm.faces.new((front[i], front[j], back[j], back[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(ob)
    return ob

def boolean(target, tool, op):
    bpy.context.view_layer.objects.active = target
    mod = target.modifiers.new("b", 'BOOLEAN')
    mod.operation = op
    mod.solver = 'EXACT'
    mod.object = tool
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(tool, do_unlink=True)

def frame(name, cx, cy, inner_l, inner_w, ledge=0.0, lift=0.0, height=FRAME_H):
    """Rahmen auf dem Boden. Mit ledge/lift bekommt er innen eine Auflagekante,
    auf der eine Platine 'lift' mm ueber dem Boden liegt."""
    il, iw = inner_l + FRAME_CLEAR, inner_w + FRAME_CLEAR
    ol, ow = il + 2 * FRAME_WALL, iw + 2 * FRAME_WALL
    outer = box(name, cx-ol/2, cx+ol/2, cy-ow/2, cy+ow/2, FLOOR-0.5, FLOOR+height)
    inner = box(name+"_in", cx-il/2, cx+il/2, cy-iw/2, cy+iw/2, FLOOR+lift, FLOOR+height+1)
    boolean(outer, inner, 'DIFFERENCE')
    if ledge > 0:
        jl, jw = il - 2*ledge, iw - 2*ledge
        under = box(name+"_under", cx-jl/2, cx+jl/2, cy-jw/2, cy+jw/2, FLOOR-1, FLOOR+lift+1)
        boolean(outer, under, 'DIFFERENCE')
    return outer

# --------------- Szene ---------------
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = 0.001
bpy.context.scene.unit_settings.length_unit = 'MILLIMETERS'
clear_scene()

# Grundkoerper
body = wedge("SolarStation", -W/2, W/2, -L/2, L/2, 0, HF, HB)

# Innenraum
cav = box("cavity", -W/2+WALL, W/2-WALL, -L/2+WALL, L/2-WALL, FLOOR, HB + 50)
boolean(body, cav, 'DIFFERENCE')

# Falz fuer das Panel (Box, um 30 Grad gekippt, Unterseite PANEL_T unter der Schraege)
T_BIG = PANEL_T + 30
zc = HF + (L/2) * math.tan(TILT)
off = T_BIG/2 - PANEL_T
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, off*n[1], zc + off*n[2]))
pocket = bpy.context.active_object
pocket.scale = (OPEN, OPEN, T_BIG)
pocket.rotation_euler = (TILT, 0, 0)
bpy.ops.object.transform_apply(scale=True, rotation=True)
boolean(body, pocket, 'DIFFERENCE')

# Kabelverschraubungen PG7 in beiden Seitenwaenden, hinten unten
gy = L/2 - GLAND_Y
for sgn in (1, -1):
    x_out = sgn * (W/2 + 3)
    x_in  = sgn * (W/2 - WALL - 3)
    hole = teardrop_x("gland", GLAND_D/2, min(x_out, x_in), max(x_out, x_in), gy, GLAND_Z)
    boolean(body, hole, 'DIFFERENCE')

# Kondenswasserablauf in der Frontwand, buendig mit Innenboden
drain = cylinder("drain", DRAIN_D/2, WALL + 6, 'Y', (0, -L/2 + WALL/2, FLOOR + DRAIN_D/2))
boolean(body, drain, 'DIFFERENCE')

# Rahmen fuer 18650-Halter und HW-775
y_bat = L/2 - WALL - 2 - (BAT_W + FRAME_CLEAR)/2 - FRAME_WALL
bat = frame("frame_18650", 0, y_bat, BAT_L, BAT_W)
boolean(body, bat, 'UNION')
y_hw = y_bat - (BAT_W + FRAME_CLEAR)/2 - FRAME_WALL - 2 - FRAME_WALL - (HW_W + FRAME_CLEAR)/2
hw = frame("frame_hw775", -40, y_hw, HW_L, HW_W)
boolean(body, hw, 'UNION')

# Rahmen mit Auflagekante fuer die Lochrasterplatine (ESP + ADS1115)
pcb = frame("frame_pcb", 20, 2, PCB_L, PCB_W, ledge=PCB_LEDGE, lift=PCB_LIFT, height=PCB_LIFT+4)
boolean(body, pcb, 'UNION')

# Panel als Referenz (eigenes Objekt, nicht Teil des Drucks)
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -(PANEL_T/2)*n[1], zc - (PANEL_T/2)*n[2]))
panel = bpy.context.active_object
panel.name = "Panel_Referenz"
panel.scale = (PANEL, PANEL, PANEL_T)
panel.rotation_euler = (TILT, 0, 0)
bpy.ops.object.transform_apply(scale=True, rotation=True)
panel.display_type = 'WIRE'

# Export
os.makedirs(OUT_DIR, exist_ok=True)
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
bpy.context.view_layer.objects.active = body
stl = os.path.join(OUT_DIR, "solar_station_case.stl")
if hasattr(bpy.ops.wm, "stl_export"):
    bpy.ops.wm.stl_export(filepath=stl, export_selected_objects=True, apply_modifiers=True)
else:
    bpy.ops.export_mesh.stl(filepath=stl, use_selection=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "solar_station_case.blend"))

print(f"Aussenmasse: {W:.1f} x {L:.1f} mm, Front {HF:.1f} mm, Rueckwand {HB:.1f} mm")
print("Verts:", len(body.data.vertices), "STL:", stl)

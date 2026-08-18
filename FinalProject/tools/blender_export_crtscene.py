"""
Blender -> .crtscene exporter.

Usage: open Blender's Scripting workspace, load this file (or paste its contents into a new
text block), edit OUTPUT_PATH below, then Run Script (Alt+P). Walks the current scene's active
camera, lights, and mesh objects and writes JSON matching the schema this raytracer's
loadScene() parses (see Homework14/main.cpp / FinalProject/main.cpp).

This has NOT been run against a real Blender install - I don't have one available to test with
in this environment. Try it on something simple (one cube, one light, the default camera)
first, and check the render looks right before trusting it on anything bigger.

Known limitations (heuristic / approximate - worth checking the output JSON afterward):
  - Material type (diffuse/reflective/refractive) is guessed from the object's first material
    slot's Principled BSDF inputs (Transmission -> refractive, Metallic -> reflective,
    otherwise diffuse). Blender's shader graph is far richer than this engine's 3-type model,
    so double-check/adjust material "type" and "albedo" in the exported JSON if it looks wrong.
  - Only one material per object is exported (the first slot) - this engine has no per-face
    material support. Split objects with multiple materials into separate objects in Blender
    first if you need that.
  - Light "intensity" is passed through directly from Blender's light "power" (Watts), but this
    engine's falloff formula (intensity / (4*pi*d^2)) uses different units than Blender's - you
    will almost certainly need to scale these by trial and error after a first test render.
  - Background color is read from the World's Background node if present; if your World uses a
    more complex node setup this will likely be wrong - just edit "background_color" by hand.
  - No UV / texture export in this version.
"""
import bpy
import json

OUTPUT_PATH = "//exported_scene.crtscene"  # "//" = relative to the .blend file; change as needed


# ---------------------------------------------------------------------------
# Axis conversion: Blender is Z-up (X right, Y forward, Z up); this engine is Y-up, matching
# every existing .crtscene scene (e.g. the dragon stands upright along Y). Standard convention
# for exporting out of Blender to a Y-up engine: (x, y, z) -> (x, z, -y). Applied to every
# position and direction below so points, normals, and camera basis vectors all stay consistent
# with each other (it's a proper rotation - orthonormality and handedness are preserved).
def convert_vec(v):
    return [v.x, v.z, -v.y]


def get_background_color():
    world = bpy.context.scene.world
    if world and world.use_nodes:
        bg = world.node_tree.nodes.get("Background")
        if bg is not None:
            c = bg.inputs[0].default_value
            return [c[0], c[1], c[2]]
    if world:
        return [world.color[0], world.color[1], world.color[2]]
    return [0.0, 0.0, 0.0]


def export_camera():
    cam_obj = bpy.context.scene.camera
    if cam_obj is None:
        raise RuntimeError("No active camera in the scene (Scene Properties > Camera).")
    m = cam_obj.matrix_world
    right = m.col[0].xyz
    up = m.col[1].xyz
    back = m.col[2].xyz  # Blender camera looks down local -Z, so col[2] points *backward*
    matrix = convert_vec(right) + convert_vec(up) + convert_vec(back)
    position = convert_vec(m.translation)
    return {"matrix": matrix, "position": position}


def export_lights():
    lights = []
    for obj in bpy.context.scene.objects:
        if obj.type != 'LIGHT':
            continue
        pos = convert_vec(obj.matrix_world.translation)
        intensity = obj.data.energy  # Blender Watts - not the same unit this engine uses
        lights.append({"position": pos, "intensity": intensity})
    return lights


def guess_material(mat):
    result = {"type": "diffuse", "albedo": [0.8, 0.8, 0.8], "smooth_shading": False}
    if mat is None:
        return result
    bsdf = mat.node_tree.nodes.get("Principled BSDF") if mat.use_nodes else None
    if bsdf is not None:
        base_color = bsdf.inputs.get("Base Color")
        if base_color is not None:
            c = base_color.default_value
            result["albedo"] = [c[0], c[1], c[2]]
        # Blender 4.x renamed "Transmission" -> "Transmission Weight"; try both.
        transmission_input = bsdf.inputs.get("Transmission Weight") or bsdf.inputs.get("Transmission")
        metallic_input = bsdf.inputs.get("Metallic")
        transmission = transmission_input.default_value if transmission_input else 0.0
        metallic = metallic_input.default_value if metallic_input else 0.0
        if transmission > 0.5:
            result["type"] = "refractive"
            ior_input = bsdf.inputs.get("IOR")
            result["ior"] = ior_input.default_value if ior_input else 1.5
            result["albedo"] = [1.0, 1.0, 1.0]
        elif metallic > 0.5:
            result["type"] = "reflective"
    else:
        base_color = getattr(mat, "diffuse_color", None)
        if base_color:
            result["albedo"] = [base_color[0], base_color[1], base_color[2]]
    return result


def export_objects(materials_out, material_lookup):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    objects_out = []
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue

        eval_obj = obj.evaluated_get(depsgraph)
        mesh = eval_obj.to_mesh()
        mesh.calc_loop_triangles()

        mat = obj.active_material
        mat_key = mat.name if mat is not None else None
        if mat_key not in material_lookup:
            material_lookup[mat_key] = len(materials_out)
            materials_out.append(guess_material(mat))
        material_index = material_lookup[mat_key]

        vertices = []
        for v in mesh.vertices:
            world_co = obj.matrix_world @ v.co
            vertices.extend(convert_vec(world_co))

        triangles = []
        for tri in mesh.loop_triangles:
            triangles.extend(tri.vertices)

        # Object-level "shade smooth" -> smooth_shading on its material. Approximate if the
        # object shares a material with another object that isn't also all-smooth.
        if mesh.polygons and all(p.use_smooth for p in mesh.polygons):
            materials_out[material_index]["smooth_shading"] = True

        objects_out.append({
            "material_index": material_index,
            "vertices": vertices,
            "triangles": triangles,
        })

        eval_obj.to_mesh_clear()

    return objects_out


def main():
    scene = bpy.context.scene
    materials_out = []
    material_lookup = {}

    objects = export_objects(materials_out, material_lookup)

    data = {
        "settings": {
            "background_color": get_background_color(),
            "image_settings": {
                "width": scene.render.resolution_x,
                "height": scene.render.resolution_y,
            },
        },
        "camera": export_camera(),
        "lights": export_lights(),
        "materials": materials_out,
        "objects": objects,
    }

    output_path = bpy.path.abspath(OUTPUT_PATH)
    with open(output_path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"Wrote {output_path} ({len(objects)} objects, {len(materials_out)} materials, {len(data['lights'])} lights)")


main()

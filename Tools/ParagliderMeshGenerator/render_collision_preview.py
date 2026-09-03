from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def parse_args() -> argparse.Namespace:
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--blend")
    parser.add_argument("--visual-obj")
    parser.add_argument("--collision-obj", required=True)
    parser.add_argument("--grabs-obj")
    parser.add_argument("--output", required=True)
    parser.add_argument("--view", choices=("iso", "front", "side"), default="iso")
    return parser.parse_args(arguments)


def material(name: str, color: tuple[float, float, float, float], emission: float = 0.0):
    result = bpy.data.materials.new(name)
    result.diffuse_color = color
    result.use_nodes = True
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = 0.65
    if emission > 0.0:
        shader.inputs["Emission Color"].default_value = color
        shader.inputs["Emission Strength"].default_value = emission
    return result


def main() -> None:
    args = parse_args()
    if args.visual_obj:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.wm.obj_import(filepath=str(Path(args.visual_obj).resolve()))
        visual_objects = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    elif args.blend:
        bpy.ops.wm.open_mainfile(filepath=str(Path(args.blend).resolve()))
        selected = bpy.data.objects.get("ParaGlider_Game")
        if selected is None:
            selected = next(obj for obj in bpy.context.scene.objects if obj.type == "MESH")
        visual_objects = [selected]
        for obj in list(bpy.context.scene.objects):
            if obj != selected:
                bpy.data.objects.remove(obj, do_unlink=True)
    else:
        raise ValueError("Pass --visual-obj or --blend")
    visual_material = material("Visual", (0.08, 0.12, 0.10, 1.0))
    for visual in visual_objects:
        visual.data.materials.clear()
        visual.data.materials.append(visual_material)

    bpy.ops.wm.obj_import(filepath=str(Path(args.collision_obj).resolve()))
    collision_objects = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    collision_material = material("Collision", (1.0, 0.02, 0.01, 1.0), emission=2.0)
    for obj in collision_objects:
        obj.data.materials.clear()
        obj.data.materials.append(collision_material)
        modifier = obj.modifiers.new("CollisionWire", "WIREFRAME")
        modifier.thickness = 0.12
        modifier.use_replace = True

    if args.grabs_obj:
        bpy.ops.wm.obj_import(filepath=str(Path(args.grabs_obj).resolve()))
        grab_objects = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
        grab_material = material("Grabs", (0.02, 0.25, 1.0, 1.0), emission=3.0)
        for obj in grab_objects:
            obj.data.materials.clear()
            obj.data.materials.append(grab_material)

    world_points = [obj.matrix_world @ vertex.co for obj in visual_objects for vertex in obj.data.vertices]
    minimum = Vector(tuple(min(point[axis] for point in world_points) for axis in range(3)))
    maximum = Vector(tuple(max(point[axis] for point in world_points) for axis in range(3)))
    center = (minimum + maximum) * 0.5
    size = (maximum - minimum).length

    camera_data = bpy.data.cameras.new("Camera")
    camera = bpy.data.objects.new("Camera", camera_data)
    bpy.context.collection.objects.link(camera)
    bpy.context.scene.camera = camera
    camera_offsets = {
        "iso": Vector((size * 1.05, -size * 1.15, size * 0.75)),
        "front": Vector((0.0, -size * 1.65, size * 0.15)),
        "side": Vector((size * 1.65, 0.0, size * 0.15)),
    }
    camera.location = center + camera_offsets[args.view]
    direction = center - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 52.0

    light_data = bpy.data.lights.new("Key", "AREA")
    light_data.energy = 1400.0
    light_data.shape = "DISK"
    light_data.size = size
    light = bpy.data.objects.new("Key", light_data)
    bpy.context.collection.objects.link(light)
    light.location = center + Vector((-size * 0.5, -size * 0.5, size * 1.3))
    light.rotation_euler = (math.radians(25.0), 0.0, math.radians(-35.0))

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1000
    scene.render.resolution_y = 850
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(Path(args.output).resolve())
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.color = (0.015, 0.015, 0.02)
    scene.render.film_transparent = False
    bpy.ops.render.render(write_still=True)
    print(f"Rendered {scene.render.filepath}")


if __name__ == "__main__":
    main()

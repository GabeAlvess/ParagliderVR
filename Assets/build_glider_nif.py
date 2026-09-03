import bpy
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path
from mathutils import Vector


MATERIALS = {
    "Material.003": {"name": "GliderCloth", "ratio": 0.14, "color": (0.30, 0.14, 0.055, 1.0), "texture": "GliderCloth_d.dds", "normal": "GliderCloth_n.dds", "normal_strength": 0.08, "roughness": 0.82},
    "Material.004": {"name": "GliderWood", "ratio": 0.18, "color": (0.32, 0.13, 0.045, 1.0), "texture": "GliderWood_d.dds", "normal": "GliderWood_n.dds", "normal_strength": 0.42, "roughness": 0.58},
    "Material.005": {"name": "GliderRope", "ratio": 0.08, "color": (0.44, 0.27, 0.12, 1.0), "texture": "GliderRope_d.dds", "normal": "GliderRope_n.dds", "normal_strength": 0.60, "roughness": 0.78},
}


def texconv_path():
    found = shutil.which("texconv") or shutil.which("texconv.exe")
    if found:
        return Path(found)
    fallback = Path.home() / "AppData/Local/Microsoft/WinGet/Packages/Microsoft.DirectXTex.Texconv_Microsoft.Winget.Source_8wekyb3d8bbwe/texconv.exe"
    if fallback.exists():
        return fallback
    raise FileNotFoundError("texconv.exe was not found")


def save_png(path, pixels, size):
    image = bpy.data.images.new(path.stem, width=size, height=size, alpha=True)
    image.pixels = pixels
    image.filepath_raw = str(path)
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)


def make_texture_pair(texture_dir, stem, base_color, height_fn, color_fn, size=512):
    heights = []
    diffuse_pixels = []
    for y in range(size):
        v = y / size
        for x in range(size):
            u = x / size
            height = max(0.0, min(1.0, height_fn(u, v)))
            heights.append(height)
            color = color_fn(u, v, height, base_color)
            diffuse_pixels.extend((*color, 1.0))

    normal_pixels = []
    strength = 3.0
    for y in range(size):
        for x in range(size):
            left = heights[y * size + ((x - 1) % size)]
            right = heights[y * size + ((x + 1) % size)]
            down = heights[((y - 1) % size) * size + x]
            up = heights[((y + 1) % size) * size + x]
            nx = (left - right) * strength
            ny = (down - up) * strength
            nz = 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            normal_pixels.extend((nx / length * 0.5 + 0.5, ny / length * 0.5 + 0.5, nz / length * 0.5 + 0.5, 1.0))

    diffuse_png = texture_dir / f"{stem}_d.png"
    normal_png = texture_dir / f"{stem}_n.png"
    save_png(diffuse_png, diffuse_pixels, size)
    save_png(normal_png, normal_pixels, size)
    return diffuse_png, normal_png


def convert_texture(converter, source, output_dir, fmt, width=None, height=None):
    command = [str(converter), "-y", "-m", "0", "-f", fmt]
    if width is not None:
        command.extend(["-w", str(width)])
    if height is not None:
        command.extend(["-h", str(height)])
    command.extend(["-o", str(output_dir), str(source)])
    subprocess.run(
        command,
        check=True,
        capture_output=True,
        text=True,
    )
    output = output_dir / f"{source.stem}.dds"
    if not output.exists():
        raise FileNotFoundError(f"texconv did not create {output}")
    return output


def prepare_textures(texture_dir):
    texture_dir.mkdir(parents=True, exist_ok=True)
    converter = texconv_path()

    cloth = make_texture_pair(
        texture_dir,
        "GliderCloth",
        MATERIALS["Material.003"]["color"][:3],
        lambda u, v: 0.5 + 0.035 * math.sin(math.tau * u * 64) + 0.035 * math.sin(math.tau * v * 64),
        lambda u, v, h, base: tuple(max(0.0, min(1.0, channel * (0.96 + 0.08 * h))) for channel in base),
    )
    wood = make_texture_pair(
        texture_dir,
        "GliderWood",
        MATERIALS["Material.004"]["color"][:3],
        lambda u, v: 0.5 + 0.28 * math.sin(math.tau * (u * 9 + 0.22 * math.sin(math.tau * v * 2))) + 0.10 * math.sin(math.tau * u * 43),
        lambda u, v, h, base: (
            max(0.0, min(1.0, base[0] * (0.62 + 0.72 * h))),
            max(0.0, min(1.0, base[1] * (0.58 + 0.64 * h))),
            max(0.0, min(1.0, base[2] * (0.55 + 0.55 * h))),
        ),
    )
    rope = make_texture_pair(
        texture_dir,
        "GliderRope",
        MATERIALS["Material.005"]["color"][:3],
        lambda u, v: 0.5 + 0.30 * math.sin(math.tau * (u * 18 + v * 18)) + 0.14 * math.sin(math.tau * (u * 36 - v * 36)),
        lambda u, v, h, base: tuple(max(0.0, min(1.0, channel * (0.66 + 0.66 * h))) for channel in base),
    )

    outputs = {}
    for diffuse_png, normal_png in (cloth, wood, rope):
        outputs[diffuse_png.with_suffix(".dds").name] = convert_texture(converter, diffuse_png, texture_dir, "BC1_UNORM_SRGB")
        outputs[normal_png.with_suffix(".dds").name] = convert_texture(converter, normal_png, texture_dir, "BC5_UNORM")
        diffuse_png.unlink(missing_ok=True)
        normal_png.unlink(missing_ok=True)
    return outputs


def make_material(name, diffuse_path, normal_path, color, normal_strength, roughness):
    material = bpy.data.materials.new(name=name)
    material.use_nodes = True
    material.diffuse_color = color
    material.use_backface_culling = False
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = 0.0
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    diffuse_image = bpy.data.images.load(str(diffuse_path), check_existing=False)
    diffuse_node = nodes.new("ShaderNodeTexImage")
    diffuse_node.name = "Base Color"
    diffuse_node.label = "Base Color"
    diffuse_node.image = diffuse_image
    links.new(diffuse_node.outputs["Color"], shader.inputs["Base Color"])
    normal_image = bpy.data.images.load(str(normal_path), check_existing=True)
    normal_image.colorspace_settings.name = "Non-Color"
    normal_node = nodes.new("ShaderNodeTexImage")
    normal_node.name = "Normal"
    normal_node.label = "Normal"
    normal_node.image = normal_image
    normal_map = nodes.new("ShaderNodeNormalMap")
    normal_map.inputs["Strength"].default_value = normal_strength
    links.new(normal_node.outputs["Color"], normal_map.inputs["Color"])
    links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])
    return material


def mesh_stats(objects):
    return {
        "objects": len(objects),
        "vertices": sum(len(obj.data.vertices) for obj in objects),
        "polygons": sum(len(obj.data.polygons) for obj in objects),
        "triangles": sum(len(obj.data.loop_triangles) for obj in objects),
    }


def look_at(obj, target):
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def render_preview(objects, output_path):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 900
    scene.render.resolution_y = 700
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(output_path)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("PreviewWorld")
    scene.world.color = (0.035, 0.035, 0.035)
    corners = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    minimum = Vector((min(v.x for v in corners), min(v.y for v in corners), min(v.z for v in corners)))
    maximum = Vector((max(v.x for v in corners), max(v.y for v in corners), max(v.z for v in corners)))
    center = (minimum + maximum) * 0.5
    span = max(maximum - minimum)
    camera_data = bpy.data.cameras.new("PreviewCamera")
    camera = bpy.data.objects.new("PreviewCamera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = center + Vector((span * 1.15, -span * 1.65, span * 0.20))
    camera_data.lens = 55
    look_at(camera, center)
    scene.camera = camera
    for name, offset, energy, size in (
        ("Key", (1.2, -1.0, 1.8), 1700, 5.0),
        ("Fill", (-1.6, -0.4, 0.8), 900, 4.0),
        ("Rim", (0.2, 1.5, 1.4), 1300, 3.0),
        ("Bounce", (0.0, -0.3, -1.2), 1200, 4.0),
    ):
        light_data = bpy.data.lights.new(name=name, type="AREA")
        light_data.energy = energy
        light_data.shape = "DISK"
        light_data.size = size
        light = bpy.data.objects.new(name, light_data)
        bpy.context.collection.objects.link(light)
        light.location = center + Vector(offset) * span
        look_at(light, center)
    bpy.ops.render.render(write_still=True)


def validate_nif(nif_path):
    from io_scene_nifly.pyn.pynifly import NifFile
    nif = NifFile(str(nif_path))
    return {
        "game": nif.game,
        "root_name": nif.rootName,
        "root_block": nif.root.blockname,
        "shape_count": len(nif.shapes),
        "shapes": [
            {
                "name": shape.name,
                "block": shape.blockname,
                "vertices": len(shape.verts),
                "triangles": len(shape.tris),
                "textures": dict(shape.shader.textures) if shape.shader else {},
            }
            for shape in nif.shapes
        ],
    }


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    source_fbx = Path(args[0]).resolve()
    assets_dir = Path(args[1]).resolve()
    source_dir = source_fbx.parent
    meshes_dir = assets_dir / "meshes/Paraglider"
    textures_dir = assets_dir / "textures/Paraglider"
    source_output_dir = assets_dir / "source"
    meshes_dir.mkdir(parents=True, exist_ok=True)
    source_output_dir.mkdir(parents=True, exist_ok=True)
    texture_paths = prepare_textures(textures_dir)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.preferences.addon_enable(module="io_scene_nifly")
    bpy.ops.import_scene.fbx(filepath=str(source_fbx), use_anim=False)
    source_object = next(obj for obj in bpy.context.scene.objects if obj.type == "MESH")
    source_object.name = "Glider"
    source_object.location = (0.0, 0.0, 0.0)
    source_object.scale *= 10.0
    bpy.context.view_layer.objects.active = source_object
    source_object.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    material_instances = {
        original_name: make_material(
            settings["name"],
            texture_paths[settings["texture"]],
            texture_paths[settings["normal"]],
            settings["color"],
            settings["normal_strength"],
            settings["roughness"],
        )
        for original_name, settings in MATERIALS.items()
    }
    for index, slot in enumerate(source_object.material_slots):
        original_name = slot.material.name if slot.material else f"slot_{index}"
        if original_name in material_instances:
            source_object.data.materials[index] = material_instances[original_name]
    source_object.data.calc_loop_triangles()
    original_stats = mesh_stats([source_object])
    bpy.context.view_layer.objects.active = source_object
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="MATERIAL")
    bpy.ops.object.mode_set(mode="OBJECT")
    mesh_objects = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    settings_by_new_name = {value["name"]: value for value in MATERIALS.values()}
    for obj in mesh_objects:
        active_material = next((slot.material for slot in obj.material_slots if slot.material), None)
        if active_material is None:
            raise RuntimeError(f"{obj.name} has no material after separation")
        settings = settings_by_new_name[active_material.name]
        obj.name = settings["name"]
        obj.data.name = f"{settings['name']}Mesh"
        obj["PYN_GAME"] = "SKYRIMSE"
        obj["PYN_BLENDER_XF"] = True
        modifier = obj.modifiers.new(name="SkyrimVR_Simplify", type="DECIMATE")
        modifier.decimate_type = "COLLAPSE"
        modifier.ratio = settings["ratio"]
        modifier.use_collapse_triangulate = True
        modifier.delimit = {"UV"}
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        triangulate = obj.modifiers.new(name="SkyrimVR_Triangulate", type="TRIANGULATE")
        triangulate.keep_custom_normals = True
        bpy.ops.object.modifier_apply(modifier=triangulate.name)
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
        obj.data.calc_loop_triangles()
    optimized_stats = mesh_stats(mesh_objects)
    root = bpy.data.objects.new("Glider", None)
    bpy.context.collection.objects.link(root)
    root["pynRoot"] = True
    root["pynBlockName"] = "BSFadeNode"
    root["pynNodeFlags"] = "SELECTIVE_UPDATE | SELECTIVE_UPDATE_TRANSFORMS | SELECTIVE_UPDATE_CONTROLLER"
    root["PYN_GAME"] = "SKYRIMSE"
    root["PYN_BLENDER_XF"] = True
    for obj in mesh_objects:
        obj.parent = root
    preview_path = assets_dir / "Glider_preview.png"
    render_preview(mesh_objects, preview_path)
    blend_path = source_output_dir / "Glider_optimized.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    blend_backup = blend_path.with_suffix(".blend1")
    blend_backup.unlink(missing_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    root.select_set(True)
    for obj in mesh_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = root
    nif_path = meshes_dir / "Glider.nif"
    result = bpy.ops.export_scene.pynifly(
        filepath=str(nif_path),
        target_game="SKYRIMSE",
        blender_xf=True,
        rename_bones=False,
        rename_bones_niftools=False,
        preserve_hierarchy=True,
        write_bodytri=False,
        export_pose=False,
        export_modifiers=False,
        export_animations=False,
        export_colors=False,
        intuit_defaults=False,
    )
    if not nif_path.exists() or nif_path.stat().st_size == 0:
        raise RuntimeError(f"NIF export failed: {result}")
    validation = validate_nif(nif_path)
    report = {
        "source": str(source_fbx),
        "nif": str(nif_path),
        "blend": str(blend_path),
        "preview": str(preview_path),
        "original": original_stats,
        "optimized": optimized_stats,
        "reduction_percent": round(100.0 * (1.0 - optimized_stats["vertices"] / original_stats["vertices"]), 2),
        "validation": validation,
    }
    (assets_dir / "Glider_build_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("GLIDER_BUILD_REPORT=" + json.dumps(report))


if __name__ == "__main__":
    main()

from __future__ import annotations

import argparse
import math
import os
import sys
from pathlib import Path


LEFT_GRAB = (-22.834, -0.048, -0.175)
RIGHT_GRAB = (-0.001, -0.009, -0.168)
HAVOK_SCALE = 69.99125


def find_pynifly_root(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    if configured := os.environ.get("PYNIFLY_ROOT"):
        candidates.append(Path(configured))
    blender_root = Path(os.environ.get("APPDATA", "")) / "Blender Foundation" / "Blender"
    if blender_root.exists():
        candidates.extend(sorted(blender_root.glob("*/scripts/addons/io_scene_nifly"), reverse=True))
    for candidate in candidates:
        if (candidate / "pyn" / "pynifly.py").is_file():
            return candidate.resolve()
    raise FileNotFoundError("PyNifly was not found. Pass --pynifly-root or set PYNIFLY_ROOT.")


def transformed_vertices(shape) -> list[tuple[float, float, float]]:
    transform = shape.transform
    return [
        tuple(
            transform.translation[axis]
            + transform.scale
            * sum(transform.rotation[axis][component] * vertex[component] for component in range(3))
            for axis in range(3)
        )
        for vertex in shape.verts
    ]


def connected_components(shape) -> list[list[tuple[float, float, float]]]:
    vertices = transformed_vertices(shape)
    parents = list(range(len(vertices)))

    def find(index: int) -> int:
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index

    def union(first: int, second: int) -> None:
        first_root = find(first)
        second_root = find(second)
        if first_root != second_root:
            parents[second_root] = first_root

    for triangle in shape.tris:
        union(triangle[0], triangle[1])
        union(triangle[1], triangle[2])

    components: dict[int, list[tuple[float, float, float]]] = {}
    for index, vertex in enumerate(vertices):
        components.setdefault(find(index), []).append(vertex)
    return sorted(components.values(), key=len, reverse=True)


def solve_3x3(matrix: list[list[float]], values: list[float]) -> tuple[float, float, float]:
    augmented = [row[:] + [value] for row, value in zip(matrix, values)]
    for column in range(3):
        pivot = max(range(column, 3), key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) < 1.0e-8:
            raise RuntimeError("Unable to fit collision panel plane")
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        divisor = augmented[column][column]
        augmented[column] = [value / divisor for value in augmented[column]]
        for row in range(3):
            if row == column:
                continue
            factor = augmented[row][column]
            augmented[row] = [
                augmented[row][index] - factor * augmented[column][index]
                for index in range(4)
            ]
    return tuple(augmented[index][3] for index in range(3))


def fit_plane(points: list[tuple[float, float, float]]) -> tuple[float, float, float]:
    count = float(len(points))
    sum_x = sum(point[0] for point in points)
    sum_y = sum(point[1] for point in points)
    sum_z = sum(point[2] for point in points)
    sum_xx = sum(point[0] * point[0] for point in points)
    sum_xy = sum(point[0] * point[1] for point in points)
    sum_yy = sum(point[1] * point[1] for point in points)
    sum_xz = sum(point[0] * point[2] for point in points)
    sum_yz = sum(point[1] * point[2] for point in points)
    return solve_3x3(
        [[sum_xx, sum_xy, sum_x], [sum_xy, sum_yy, sum_y], [sum_x, sum_y, count]],
        [sum_xz, sum_yz, sum_z],
    )


def normalize(vector: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(sum(component * component for component in vector))
    return tuple(component / length for component in vector)


def subtract(first, second) -> tuple[float, float, float]:
    return tuple(first[index] - second[index] for index in range(3))


def cross(first, second) -> tuple[float, float, float]:
    return (
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0],
    )


def build_convex(vertices, faces) -> dict:
    center = tuple(sum(vertex[index] for vertex in vertices) / len(vertices) for index in range(3))
    corrected_faces = []
    normals = []
    for face in faces:
        face_vertices = [vertices[index] for index in face]
        face_normal = normalize(cross(subtract(face_vertices[1], face_vertices[0]), subtract(face_vertices[2], face_vertices[0])))
        face_center = tuple(sum(vertex[index] for vertex in face_vertices) / len(face_vertices) for index in range(3))
        if sum(face_normal[index] * (face_center[index] - center[index]) for index in range(3)) < 0.0:
            face = tuple(reversed(face))
            face_normal = tuple(-component for component in face_normal)
        corrected_faces.append(face)
        distance = -sum(face_normal[index] * face_vertices[0][index] for index in range(3))
        normals.append((*face_normal, distance))
    return {"vertices": vertices, "faces": corrected_faces, "normals": normals}


def edge_profile(component, x: float, window: float) -> tuple[float, float]:
    nearby = [point for point in component if abs(point[0] - x) <= window]
    if len(nearby) < 6:
        nearby = sorted(component, key=lambda point: abs(point[0] - x))[:12]
    return min(point[1] for point in nearby), max(point[1] for point in nearby)


def create_cloth_panel(component, thickness: float, padding: float) -> dict:
    plane_a, plane_b, plane_c = fit_plane(component)
    extrema = [
        min(component, key=lambda point: point[0]),
        max(component, key=lambda point: point[1]),
        max(component, key=lambda point: point[0]),
        min(component, key=lambda point: point[1]),
    ]
    center_x = sum(point[0] for point in extrema) / 4.0
    center_y = sum(point[1] for point in extrema) / 4.0
    normal = normalize((-plane_a, -plane_b, 1.0))
    surface = []
    for point in extrema:
        x, y = point[0], point[1]
        delta_x = x - center_x
        delta_y = y - center_y
        planar_length = math.hypot(delta_x, delta_y)
        if planar_length > 1.0e-6:
            x += padding * delta_x / planar_length
            y += padding * delta_y / planar_length
        surface.append((x, y, plane_a * x + plane_b * y + plane_c))
    half_thickness = thickness * 0.5
    vertices = [
        tuple(point[index] + normal[index] * offset for index in range(3))
        for offset in (-half_thickness, half_thickness)
        for point in surface
    ]
    panel = build_convex(
        vertices,
        [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)],
    )
    panel["kind"] = "cloth"
    return panel


def create_segment_box(start, end, width: float, depth: float, kind: str) -> dict:
    direction = normalize(subtract(end, start))
    reference = (0.0, 0.0, 1.0)
    if abs(sum(direction[index] * reference[index] for index in range(3))) > 0.92:
        reference = (0.0, 1.0, 0.0)
    side = normalize(cross(direction, reference))
    up = normalize(cross(side, direction))
    half_width = width * 0.5
    half_depth = depth * 0.5
    vertices = []
    for point in (start, end):
        for side_sign, up_sign in ((-1.0, -1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)):
            vertices.append(tuple(
                point[index] + side[index] * half_width * side_sign + up[index] * half_depth * up_sign
                for index in range(3)
            ))
    box = build_convex(
        vertices,
        [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)],
    )
    box["kind"] = kind
    return box


def lateral_structure_components(components) -> list[list[tuple[float, float, float]]]:
    candidates = []
    for component in components:
        extents = [
            max(point[axis] for point in component) - min(point[axis] for point in component)
            for axis in range(3)
        ]
        if extents[0] < 12.0 and extents[1] > 40.0 and extents[2] > 15.0:
            candidates.append(component)
    if len(candidates) != 2:
        raise RuntimeError(f"Expected two lateral structures, found {len(candidates)}")
    return sorted(candidates, key=lambda component: sum(point[0] for point in component) / len(component))


def centerline_points(component, segment_count: int) -> list[tuple[float, float, float]]:
    extents = [
        max(point[axis] for point in component) - min(point[axis] for point in component)
        for axis in range(3)
    ]
    axis = max(range(3), key=extents.__getitem__)
    minimum = min(point[axis] for point in component)
    maximum = max(point[axis] for point in component)
    sample_count = max(16, len(component) // 12)
    points = []
    for index in range(segment_count + 1):
        target = minimum + (maximum - minimum) * index / segment_count
        nearest = sorted(component, key=lambda point: abs(point[axis] - target))[:sample_count]
        points.append(tuple(
            sum(point[coordinate] for point in nearest) / len(nearest)
            for coordinate in range(3)
        ))
    return points


def create_lateral_structure_parts(component, side_name: str, width: float) -> list[dict]:
    points = centerline_points(component, 3)
    return [
        create_segment_box(points[index], points[index + 1], width, width, f"{side_name}_structure_{index + 1}")
        for index in range(3)
    ]


def create_collision_parts(components, cloth_thickness: float, cloth_padding: float, structure_width: float) -> list[dict]:
    cloth = max(components, key=len)
    hidden_side, visible_side = lateral_structure_components(components)
    parts = [create_cloth_panel(cloth, cloth_thickness, cloth_padding)]
    parts.extend(create_lateral_structure_parts(hidden_side, "hidden", structure_width))
    parts.extend(create_lateral_structure_parts(visible_side, "visible", structure_width))
    return parts


def write_preview_obj(path: Path, panels: list[dict]) -> None:
    lines = []
    vertex_offset = 1
    for index, panel in enumerate(panels):
        lines.append(f"o ParagliderCollisionPanel_{index:02d}")
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in panel["vertices"])
        for face in panel["faces"]:
            lines.append("f " + " ".join(str(vertex_offset + vertex) for vertex in face))
        vertex_offset += len(panel["vertices"])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_grab_preview_obj(path: Path, model_scale: float, half_size: float = 1.2) -> None:
    lines = []
    vertex_offset = 1
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    for name, base_center in (("LeftGrab", LEFT_GRAB), ("RightGrab", RIGHT_GRAB)):
        center = tuple(component * model_scale for component in base_center)
        lines.append(f"o {name}")
        vertices = [
            (
                center[0] + x * half_size * model_scale,
                center[1] + y * half_size * model_scale,
                center[2] + z * half_size * model_scale,
            )
            for z in (-1.0, 1.0)
            for y in (-1.0, 1.0)
            for x in (-1.0, 1.0)
        ]
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices)
        for face in faces:
            lines.append("f " + " ".join(str(vertex_offset + vertex) for vertex in face))
        vertex_offset += len(vertices)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_visual_obj(path: Path, shapes, model_scale: float) -> None:
    lines = []
    vertex_offset = 1
    for index, shape in enumerate(shapes):
        vertices = [
            tuple(component * model_scale for component in vertex)
            for vertex in transformed_vertices(shape)
        ]
        lines.append(f"o ParagliderVisual_{index:02d}")
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices)
        for triangle in shape.tris:
            lines.append("f " + " ".join(str(vertex_offset + vertex) for vertex in triangle))
        vertex_offset += len(vertices)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def copy_shape(source, destination, parent, node_id_none, shader_flags_2, model_scale: float):
    properties = source.properties.copy()
    for attribute in ("nameID", "controllerID", "collisionID", "skinInstanceID", "shaderPropertyID", "alphaPropertyID"):
        setattr(properties, attribute, node_id_none)
    shape = destination.createShapeFromData(
        source.name, source.verts, source.tris, source.uvs, source.normals,
        props=properties, use_type=source.properties.bufType, parent=parent,
    )
    shape.flags = source.flags
    transform = source.transform
    transform.translation = tuple(component * model_scale for component in transform.translation)
    transform.scale *= model_scale
    shape.transform = transform
    shape.shader.name = source.shader.name
    shape.shader._properties = source.shader.properties.copy()
    shape.shader.properties.shaderflags2_set(shader_flags_2.DOUBLE_SIDED)
    shape.save_shader_attributes()
    for slot, texture_path in source.textures.items():
        if texture_path:
            shape.set_texture(slot, texture_path)
    if source.has_alpha_property:
        shape.has_alpha_property = True
        shape.alpha_property._properties = source.alpha_property.properties.copy()
        shape.save_alpha_property()
    return shape


def add_grab_node(destination, name, position, transform_type, model_scale: float):
    transform = transform_type()
    transform.translation = tuple(component * model_scale for component in position)
    transform.rotation = ((1.0, 0.0, 0.0), (0.0, 0.0, -1.0), (0.0, 1.0, 0.0))
    return destination.add_node(name, transform, parent=destination.rootNode)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--visual", required=True)
    parser.add_argument("--collision-template", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--preview-obj")
    parser.add_argument("--preview-visual-obj")
    parser.add_argument("--preview-grabs-obj")
    parser.add_argument("--mass", type=float, default=0.35)
    parser.add_argument("--cloth-thickness", type=float, default=0.8)
    parser.add_argument("--cloth-padding", type=float, default=0.25)
    parser.add_argument("--structure-width", type=float, default=1.8)
    parser.add_argument("--collision-margin", type=float, default=0.20)
    parser.add_argument("--model-scale", type=float, default=1.0)
    parser.add_argument("--pynifly-root")
    args = parser.parse_args()
    if args.mass <= 0.0 or args.cloth_thickness <= 0.0 or args.structure_width <= 0.0 or args.model_scale <= 0.0:
        raise ValueError("Invalid physical glider settings")

    pynifly_root = find_pynifly_root(args.pynifly_root)
    sys.path.insert(0, str(pynifly_root))
    from pyn.pynifly import BSInvMarker, BSXFlags, NifFile
    from pyn.nifdefs import NODEID_NONE, PynBufferTypes, ShaderFlags2, bhkConvexVerticesShapeProps, bhkListShapeProps
    from pyn.structs import TransformBuf

    visual_path = Path(args.visual).resolve()
    template_path = Path(args.collision_template).resolve()
    output_path = Path(args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    visual = NifFile(str(visual_path))
    if args.preview_visual_obj:
        write_visual_obj(Path(args.preview_visual_obj).resolve(), visual.shapes, args.model_scale)
    if args.preview_grabs_obj:
        write_grab_preview_obj(Path(args.preview_grabs_obj).resolve(), args.model_scale)
    components = [
        [tuple(coordinate * args.model_scale for coordinate in point) for point in component]
        for shape in visual.shapes
        for component in connected_components(shape)
    ]
    canopy = max(components, key=len)
    cloth_thickness = args.cloth_thickness * args.model_scale
    cloth_padding = args.cloth_padding * args.model_scale
    structure_width = args.structure_width * args.model_scale
    collision_margin = args.collision_margin * args.model_scale
    panels = create_collision_parts(components, cloth_thickness, cloth_padding, structure_width)
    if args.preview_obj:
        write_preview_obj(Path(args.preview_obj).resolve(), panels)

    destination = NifFile()
    destination.initialize(visual.game, str(output_path), type(visual.rootNode).__name__, visual.rootNode.name)
    destination.rootNode.flags = visual.rootNode.flags
    copied_shapes = [
        copy_shape(shape, destination, destination.rootNode, NODEID_NONE, ShaderFlags2, args.model_scale)
        for shape in visual.shapes
    ]
    template = NifFile(str(template_path))
    template_node = next(node for node in template.nodes.values() if node.collision_object)
    template_collision = template_node.collision_object
    template_body = template_collision.body
    template_shape = template_body.shape

    if template_bsx := template.rootNode.get_extra_data(blockname="BSXFlags"):
        BSXFlags.New(destination, name=template_bsx.name, flags=int(template_bsx.flags), parent=destination.rootNode)
    marker = template.rootNode.get_extra_data(blockname="BSInvMarker")
    BSInvMarker.New(
        destination, name=marker.name if marker else "INV",
        rotation=[round(math.radians(75.0) * 1000.0), 0, round(math.radians(20.0) * 1000.0)],
        zoom=0.7, parent=destination.rootNode,
    )

    all_vertices = [vertex for shape in copied_shapes for vertex in transformed_vertices(shape)]
    minimum = [min(vertex[axis] for vertex in all_vertices) for axis in range(3)]
    maximum = [max(vertex[axis] for vertex in all_vertices) for axis in range(3)]
    collision_node = destination.add_node("ParagliderCollision", TransformBuf(), parent=destination.rootNode)
    collision_node.flags = template_node.flags
    collision = collision_node.add_collision(body=None, flags=template_collision.properties.flags, collision_type=PynBufferTypes.bhkCollisionObjectBufType)
    body_properties = template_body.properties.copy()
    body_properties.shapeID = 0xFFFFFFFF
    body_properties.mass = args.mass
    for index in range(3):
        body_properties.translation[index] = 0.0
        body_properties.center[index] = ((minimum[index] + maximum[index]) * 0.5) / HAVOK_SCALE
    body_properties.translation[3] = 0.0
    body_properties.center[3] = 0.0

    width, depth, height = [(maximum[index] - minimum[index]) / HAVOK_SCALE for index in range(3)]
    inertia = [
        args.mass * (depth * depth + height * height) / 12.0,
        args.mass * (width * width + height * height) / 12.0,
        args.mass * (width * width + depth * depth) / 12.0,
    ]
    for index in range(12):
        body_properties.inertiaMatrix[index] = 0.0
    body_properties.inertiaMatrix[0] = inertia[0]
    body_properties.inertiaMatrix[5] = inertia[1]
    body_properties.inertiaMatrix[10] = inertia[2]

    body = collision.add_body(body_properties)
    list_properties = bhkListShapeProps(game=visual.game)
    list_properties.bhkMaterial = template_shape.properties.bhkMaterial
    list_properties.childCount = 0
    compound = body.add_shape(list_properties)
    for panel in panels:
        convex_properties = bhkConvexVerticesShapeProps(game=visual.game)
        convex_properties.bhkMaterial = template_shape.properties.bhkMaterial
        convex_properties.bhkRadius = min(template_shape.properties.bhkRadius, collision_margin / HAVOK_SCALE)
        vertices = [tuple(component / HAVOK_SCALE for component in vertex) for vertex in panel["vertices"]]
        normals = [(normal[0], normal[1], normal[2], normal[3] / HAVOK_SCALE) for normal in panel["normals"]]
        destination.add_shape(convex_properties, parent=compound, vertices=vertices, normals=normals)

    add_grab_node(destination, "HIGGS:GrabL", LEFT_GRAB, TransformBuf, args.model_scale)
    add_grab_node(destination, "HIGGS:GrabR", RIGHT_GRAB, TransformBuf, args.model_scale)
    destination.save()

    generated = NifFile(str(output_path))
    grab_names = {node.name for node in generated.nodes.values()}
    if not {"HIGGS:GrabL", "HIGGS:GrabR"}.issubset(grab_names):
        raise RuntimeError("Generated NIF is missing HIGGS grab nodes")
    if not all(shape.shader.flag_double_sided for shape in generated.shapes):
        raise RuntimeError("Generated NIF contains a single-sided visual shape")
    generated_body = next(node.collision_object.body for node in generated.nodes.values() if node.collision_object)
    if len(generated_body.shape.children) != len(panels):
        raise RuntimeError("Generated collision panel count is incorrect")
    if any(abs(generated_body.properties.translation[index]) > 1.0e-6 for index in range(3)):
        raise RuntimeError("Generated rigid body has a non-zero translation")

    print(f"Generated {output_path}")
    print(f"Mass: {args.mass}")
    print(f"Model scale: {args.model_scale}")
    print(f"Visual bounds: {minimum} to {maximum}")
    print(f"Canopy vertices: {len(canopy)}")
    print(f"Simple collision parts: {len(panels)} (1 cloth + 3 visible side + 3 hidden side)")
    print(f"Cloth thickness: {cloth_thickness}")
    print(f"Grip/structure width: {structure_width}")
    print(f"HIGGS left grab: {tuple(component * args.model_scale for component in LEFT_GRAB)}")
    print(f"HIGGS right grab: {tuple(component * args.model_scale for component in RIGHT_GRAB)}")


if __name__ == "__main__":
    main()

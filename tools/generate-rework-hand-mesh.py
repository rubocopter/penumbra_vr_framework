#!/usr/bin/env python3
"""Generate compact renderer data from the proven Rework hand rigs.

The generated file is checked in so normal Framework builds do not depend on
Python or Pillow. Run this tool only when the source Rework assets change.
"""

from __future__ import annotations

import argparse
import io
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "products" / "overture" / "data" / "models" / "hud_objects"
DEFAULT_OUTPUT = ROOT / "src" / "graphics" / "generated" / "rework_hand_mesh_data.inl"
RIGHT_DAE = ASSET_DIR / "hud_object_hand_rig.dae"
LEFT_DAE = ASSET_DIR / "hud_object_hand_left_rig.dae"
DIFFUSE = ASSET_DIR / "HAND_Low_C.jpg"
EXPECTED_JOINTS = [
    "Hand_Root", "Palm",
    "Little1", "Little2", "Little3",
    "Ring1", "Ring2", "Ring3",
    "Middle1", "Middle2", "Middle3",
    "Index1", "Index2", "Index3",
    "Thumb1", "Thumb2", "Thumb3",
]
TEXTURE_SIZE = 128


def namespace(root: ET.Element) -> dict[str, str]:
    if not root.tag.startswith("{"):
        raise ValueError("COLLADA namespace is missing")
    return {"c": root.tag[1 : root.tag.index("}")]}


def floats(element: ET.Element) -> list[float]:
    return [float(value) for value in (element.text or "").split()]


def ints(element: ET.Element) -> list[int]:
    return [int(value) for value in (element.text or "").split()]


def source_records(mesh: ET.Element, ns: dict[str, str], source_id: str) -> list[tuple[float, ...]]:
    source = mesh.find(f"c:source[@id='{source_id}']", ns)
    if source is None:
        raise ValueError(f"missing geometry source {source_id}")
    array = source.find("c:float_array", ns)
    accessor = source.find("c:technique_common/c:accessor", ns)
    if array is None or accessor is None:
        raise ValueError(f"incomplete geometry source {source_id}")
    stride = int(accessor.attrib.get("stride", "1"))
    values = floats(array)
    if len(values) % stride:
        raise ValueError(f"source {source_id} has a partial record")
    return [tuple(values[index : index + stride]) for index in range(0, len(values), stride)]


def parse_hand(path: Path) -> dict[str, object]:
    root = ET.parse(path).getroot()
    ns = namespace(root)
    mesh = root.find(".//c:geometry/c:mesh", ns)
    skin = root.find(".//c:controller/c:skin", ns)
    scene = root.find(".//c:library_visual_scenes/c:visual_scene", ns)
    if mesh is None or skin is None or scene is None:
        raise ValueError(f"{path.name}: expected one mesh, skin controller and visual scene")

    vertices = mesh.find("c:vertices", ns)
    triangles = mesh.find("c:triangles", ns)
    if vertices is None or triangles is None:
        raise ValueError(f"{path.name}: mesh is missing vertices/triangles")
    position_input = vertices.find("c:input[@semantic='POSITION']", ns)
    if position_input is None:
        raise ValueError(f"{path.name}: POSITION input is missing")
    position_source = position_input.attrib["source"].lstrip("#")
    positions = source_records(mesh, ns, position_source)
    if any(len(value) != 3 for value in positions):
        raise ValueError(f"{path.name}: positions must be float3")

    tri_inputs = triangles.findall("c:input", ns)
    tri_offsets = {item.attrib["semantic"]: int(item.attrib.get("offset", "0")) for item in tri_inputs}
    if "VERTEX" not in tri_offsets or "TEXCOORD" not in tri_offsets:
        raise ValueError(f"{path.name}: triangles require VERTEX and TEXCOORD inputs")
    stride = max(tri_offsets.values()) + 1
    uv_input = next(item for item in tri_inputs if item.attrib["semantic"] == "TEXCOORD")
    uvs = source_records(mesh, ns, uv_input.attrib["source"].lstrip("#"))
    if any(len(value) != 2 for value in uvs):
        raise ValueError(f"{path.name}: UVs must be float2")
    packed = ints(triangles.find("c:p", ns))
    if len(packed) % stride:
        raise ValueError(f"{path.name}: triangle index stream is incomplete")
    corners = [
        (packed[index + tri_offsets["VERTEX"]], packed[index + tri_offsets["TEXCOORD"]])
        for index in range(0, len(packed), stride)
    ]
    expected_corners = int(triangles.attrib["count"]) * 3
    if len(corners) != expected_corners:
        raise ValueError(f"{path.name}: triangle count does not match corner stream")
    if max(max(corner) for corner in corners) > 0xFFFF:
        raise ValueError(f"{path.name}: generated uint16 indices would overflow")

    joint_input = skin.find("c:joints/c:input[@semantic='JOINT']", ns)
    bind_input = skin.find("c:joints/c:input[@semantic='INV_BIND_MATRIX']", ns)
    if joint_input is None or bind_input is None:
        raise ValueError(f"{path.name}: skin joint inputs are incomplete")
    joint_source_id = joint_input.attrib["source"].lstrip("#")
    joint_source = skin.find(f"c:source[@id='{joint_source_id}']", ns)
    bind_source = skin.find(f"c:source[@id='{bind_input.attrib['source'].lstrip('#')}']", ns)
    if joint_source is None or bind_source is None:
        raise ValueError(f"{path.name}: skin sources are missing")
    joint_array = joint_source.find("c:Name_array", ns)
    bind_array = bind_source.find("c:float_array", ns)
    if joint_array is None or bind_array is None:
        raise ValueError(f"{path.name}: skin arrays are missing")
    joints = (joint_array.text or "").split()
    if joints != EXPECTED_JOINTS:
        raise ValueError(f"{path.name}: unexpected joint order {joints}")
    bind_values = floats(bind_array)
    if len(bind_values) != len(joints) * 16:
        raise ValueError(f"{path.name}: inverse-bind matrix count is invalid")
    inverse_bind = [tuple(bind_values[index : index + 16]) for index in range(0, len(bind_values), 16)]

    vertex_weights = skin.find("c:vertex_weights", ns)
    if vertex_weights is None:
        raise ValueError(f"{path.name}: vertex_weights are missing")
    weight_inputs = vertex_weights.findall("c:input", ns)
    weight_offsets = {item.attrib["semantic"]: int(item.attrib.get("offset", "0")) for item in weight_inputs}
    weight_stride = max(weight_offsets.values()) + 1
    counts = ints(vertex_weights.find("c:vcount", ns))
    weights = ints(vertex_weights.find("c:v", ns))
    if len(counts) != len(positions) or set(counts) != {1}:
        raise ValueError(f"{path.name}: the renderer requires the proven one-bone-per-position rig")
    if len(weights) != len(positions) * weight_stride:
        raise ValueError(f"{path.name}: vertex weight stream size is invalid")
    joint_offset = weight_offsets["JOINT"]
    position_bones = [weights[index * weight_stride + joint_offset] for index in range(len(positions))]
    if min(position_bones) < 0 or max(position_bones) >= len(joints):
        raise ValueError(f"{path.name}: vertex references an invalid joint")

    parent = [-1] * len(joints)
    translation: list[tuple[float, float, float] | None] = [None] * len(joints)
    joint_index = {name: index for index, name in enumerate(joints)}

    def walk(node: ET.Element, parent_joint: int) -> None:
        name = node.attrib.get("name") or node.attrib.get("id")
        current_parent = parent_joint
        if name in joint_index:
            index = joint_index[name]
            parent[index] = parent_joint
            unsupported = [
                child.tag.rsplit("}", 1)[-1]
                for child in node
                if child.tag.rsplit("}", 1)[-1] in {"matrix", "rotate", "scale"}
            ]
            if unsupported:
                raise ValueError(
                    f"{path.name}: joint {name} gained unsupported bind transforms {unsupported}"
                )
            translate = node.find("c:translate", ns)
            if translate is None:
                raise ValueError(f"{path.name}: joint {name} has no bind translation")
            values = tuple(floats(translate))
            if len(values) != 3:
                raise ValueError(f"{path.name}: joint {name} translation is not float3")
            translation[index] = values  # type: ignore[assignment]
            current_parent = index
        for child in node.findall("c:node", ns):
            walk(child, current_parent)

    for node in scene.findall("c:node", ns):
        walk(node, -1)
    if any(value is None for value in translation):
        raise ValueError(f"{path.name}: not every skin joint exists in the visual hierarchy")

    return {
        "positions": positions,
        "uvs": uvs,
        "corners": corners,
        "bones": position_bones,
        "inverse_bind": inverse_bind,
        "translation": translation,
        "parent": parent,
        "triangles": int(triangles.attrib["count"]),
    }


def f32(value: float) -> str:
    if value == 0.0:
        value = 0.0
    literal = f"{value:.9g}"
    if "." not in literal and "e" not in literal.lower():
        literal += ".0"
    return literal + "F"


def format_rows(values: list[str], per_line: int = 8, indent: str = "    ") -> str:
    lines = []
    for index in range(0, len(values), per_line):
        lines.append(indent + ", ".join(values[index : index + per_line]) + ",")
    return "\n".join(lines)


def emit_vec_array(name: str, typename: str, values: list[tuple[float, ...]]) -> str:
    rows = ["{" + ", ".join(f32(component) for component in value) + "}" for value in values]
    return f"inline constexpr std::array<{typename}, {len(rows)}> {name}{{{{\n{format_rows(rows, 3)}\n}}}};\n"


def emit_scalar_array(name: str, typename: str, values: list[int], per_line: int = 24) -> str:
    rows = [str(value) for value in values]
    return f"inline constexpr std::array<{typename}, {len(rows)}> {name}{{{{\n{format_rows(rows, per_line)}\n}}}};\n"


def emit_corner_array(name: str, values: list[tuple[int, int]]) -> str:
    rows = [f"{{{position}, {uv}}}" for position, uv in values]
    return f"inline constexpr std::array<MeshCorner, {len(rows)}> {name}{{{{\n{format_rows(rows, 6)}\n}}}};\n"


def emit_matrix_array(name: str, values: list[tuple[float, ...]]) -> str:
    rows = ["{{" + ", ".join(f32(component) for component in value) + "}}" for value in values]
    return f"inline constexpr std::array<std::array<float, 16>, {len(rows)}> {name}{{{{\n{format_rows(rows, 1)}\n}}}};\n"


def texture_rgb() -> list[int]:
    with Image.open(DIFFUSE) as source:
        image = source.convert("RGB").resize((TEXTURE_SIZE, TEXTURE_SIZE), Image.Resampling.LANCZOS)
        # OpenGL's first uploaded row is the bottom row; keep authored COLLADA
        # UVs unchanged and flip the JPEG once during generation.
        image = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        return list(image.tobytes())


def generate() -> str:
    right = parse_hand(RIGHT_DAE)
    left = parse_hand(LEFT_DAE)
    if right["uvs"] != left["uvs"]:
        raise ValueError("left/right Rework rigs unexpectedly use different UV tables")
    if right["triangles"] != 6034 or left["triangles"] != 6034:
        raise ValueError("Rework hand triangle count changed; review the asset before regenerating")

    output = io.StringIO()
    output.write("// Generated by tools/generate-rework-hand-mesh.py. Do not edit by hand.\n")
    output.write("// Source: Rework 23c890f hand DAE/material assets already carried by this repository.\n\n")
    output.write(emit_vec_array("kHandUvs", "MeshVec2", right["uvs"]))
    for prefix, hand in (("Right", right), ("Left", left)):
        output.write("\n")
        output.write(emit_vec_array(f"k{prefix}Positions", "MeshVec3", hand["positions"]))
        output.write(emit_scalar_array(f"k{prefix}PositionBones", "std::uint8_t", hand["bones"]))
        output.write(emit_corner_array(f"k{prefix}Corners", hand["corners"]))
        output.write(emit_vec_array(f"k{prefix}LocalTranslations", "MeshVec3", hand["translation"]))
        output.write(emit_scalar_array(f"k{prefix}Parents", "std::int8_t", hand["parent"], 17))
        output.write(emit_matrix_array(f"k{prefix}InverseBind", hand["inverse_bind"]))
    output.write("\n")
    output.write(f"inline constexpr int kHandTextureWidth = {TEXTURE_SIZE};\n")
    output.write(f"inline constexpr int kHandTextureHeight = {TEXTURE_SIZE};\n")
    output.write(emit_scalar_array("kHandTextureRgb", "std::uint8_t", texture_rgb(), 24))
    return output.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    content = generate()
    output = args.output.resolve()
    if args.check:
        if not output.exists() or output.read_text(encoding="utf-8") != content:
            print(f"generated Rework hand mesh is stale: {output}", file=sys.stderr)
            return 1
        print(f"generated Rework hand mesh is current: {output}")
        return 0
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(content, encoding="utf-8", newline="\n")
    print(f"generated {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

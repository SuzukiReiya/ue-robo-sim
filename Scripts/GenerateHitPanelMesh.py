"""添付STEP由来の低ポリゴン可視化メッシュをOBJに復元する。

元データ: GroundHitPanelModule.step（約90×90×19mm）。
CADから生成した三角形メッシュを約500面に簡略化し、0.1mm単位で量子化。
複雑な製造ディテールを保持するための原寸CADではなく、映像試験用の表示メッシュ。
標準ライブラリだけで復元できるためWindows側にCADツールは不要。
"""
import base64
import struct
from pathlib import Path
import zlib

_MESH = """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"""


def load_triangles():
    binary = zlib.decompress(base64.b64decode(_MESH))
    vertex_count, face_count = struct.unpack_from("<II", binary)
    assert len(binary) == 8 + 6 * (vertex_count + face_count)
    vertices = [
        struct.unpack_from("<hhh", binary, 8 + 6 * i)
        for i in range(vertex_count)
    ]
    faces_start = 8 + 6 * vertex_count
    faces = [
        struct.unpack_from("<HHH", binary, faces_start + 6 * i)
        for i in range(face_count)
    ]
    return vertices, faces


def generate_obj(path):
    """mm×10の座標をUEのcmへ換算してOBJ書き出し（ローカル+Zがパネル前面）。"""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    vertices, faces = load_triangles()
    with path.open("w", encoding="ascii", newline="\n") as out:
        out.write("# STEP-derived low-poly hit panel / units: centimeters\n")
        out.write("o GroundHitPanelModule\n")
        for x, y, z in vertices:
            out.write(f"v {x / 100:.3f} {y / 100:.3f} {z / 100:.3f}\n")
        for a, b, c in faces:
            out.write(f"f {a + 1} {b + 1} {c + 1}\n")
    return path


if __name__ == "__main__":
    generate_obj(Path(__file__).resolve().parents[1] /
                 "Saved" / "GeneratedMeshes" / "GroundHitPanelModule.obj")

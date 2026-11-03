"""STEP由来ヒットパネルOBJの形状・単位・決定性の確認（標準ライブラリのみ）。"""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))
from GenerateHitPanelMesh import load_triangles, generate_obj


def main():
    vertices, faces = load_triangles()
    assert len(vertices) >= 200
    assert len(faces) >= 450
    for triangle in faces:
        assert len(set(triangle)) == 3
        assert all(0 <= i < len(vertices) for i in triangle)
    # 内部単位0.1mm。STEPの最大外形は90×90×19.2mm程度。
    mins = [min(p[k] for p in vertices) for k in range(3)]
    maxs = [max(p[k] for p in vertices) for k in range(3)]
    assert 890 < maxs[0] - mins[0] < 910
    assert 890 < maxs[1] - mins[1] < 910
    assert 160 < maxs[2] - mins[2] < 215
    with tempfile.TemporaryDirectory() as root:
        a = generate_obj(Path(root) / "first.obj")
        b = generate_obj(Path(root) / "second.obj")
        assert a.read_bytes() == b.read_bytes()
        text = a.read_text()
        assert sum(s.startswith("v ") for s in text.splitlines()) == len(vertices)
        assert sum(s.startswith("f ") for s in text.splitlines()) == len(faces)
    print("STEP由来メッシュ生成・CAD単位・パネル寸法・再現性: 成功")


if __name__ == "__main__":
    main()

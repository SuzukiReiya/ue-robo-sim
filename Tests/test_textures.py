"""高解像度PBR画像の生成・法線・キャッシュ・再現性を検証する。"""
import struct
import sys
import tempfile
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))
from GenerateTextures import SIZE, SURFACES, MAPS, generate_all


def decode_rgb(path, expected_size):
    png = path.read_bytes()
    assert png[:8] == b"\x89PNG\r\n\x1a\n"
    offset = 8
    chunks = []
    dimensions = None
    while offset < len(png):
        count = struct.unpack_from(">I", png, offset)[0]
        kind = png[offset + 4:offset + 8]
        payload = png[offset + 8:offset + 8 + count]
        checksum = struct.unpack_from(">I", png, offset + 8 + count)[0]
        assert checksum == (zlib.crc32(kind + payload) & 0xffffffff)
        offset += count + 12
        if kind == b"IHDR":
            dimensions = struct.unpack(">2I5B", payload)
        if kind == b"IDAT":
            chunks.append(payload)
    assert dimensions == (expected_size, expected_size, 8, 2, 0, 0, 0)
    raw = zlib.decompress(b"".join(chunks))
    stride = expected_size * 3
    assert len(raw) == expected_size * (stride + 1)
    assert all(raw[y * (stride + 1)] == 0 for y in range(expected_size))
    return b"".join(
        raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)]
        for y in range(expected_size))


def main():
    assert SIZE == 2048
    with tempfile.TemporaryDirectory() as directory:
        # CIは小さいサイズで全経路を通し、実運用の2048pxは設定値を確認。
        a = generate_all(Path(directory) / "a", size=96)
        b = generate_all(Path(directory) / "b", size=96)
        assert set(a) == set(SURFACES)
        for surface in SURFACES:
            assert set(a[surface]) == set(MAPS)
            for kind in MAPS:
                path = a[surface][kind]
                other = b[surface][kind]
                assert path.read_bytes() == other.read_bytes()
                rgb = decode_rgb(path, 96)
                assert len(rgb) == 96 * 96 * 3
                assert len(set(rgb)) > 18, (surface, kind)
                if kind == "Normal":
                    # 全領域が真っ平らでないが、面の表裏反転もないこと。
                    blue = rgb[2::3]
                    assert min(blue) >= 128
                    assert max(blue) > 170
                if kind == "Roughness":
                    # 物理量として独立したグレースケール画像。
                    assert all(rgb[i] == rgb[i + 1] == rgb[i + 2]
                               for i in range(0, len(rgb), 3))
        previous = {p: p.stat().st_mtime_ns
                    for group in a.values() for p in group.values()}
        again = generate_all(Path(directory) / "a", size=96)
        assert again == a
        assert all(p.stat().st_mtime_ns == when for p, when in previous.items())
        print("PBR画像・PNG・法線・粗さ・再現性・キャッシュ: 成功")


if __name__ == "__main__":
    main()

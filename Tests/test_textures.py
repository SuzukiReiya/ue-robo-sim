"""表面画像が同一seedで再現可能かつ、非一様であることを検証。"""
import struct
import sys
import tempfile
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Scripts'))
from GenerateTextures import SIZE, generate_all


def load_rgb(path):
    png = path.read_bytes()
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    chunks = []
    offset = 8
    while offset < len(png):
        count = struct.unpack_from('>I', png, offset)[0]
        kind = png[offset + 4:offset + 8]
        data = png[offset + 8:offset + 8 + count]
        offset += count + 12
        if kind == b'IDAT':
            chunks.append(data)
    raw = zlib.decompress(b''.join(chunks))
    assert len(raw) == SIZE * (1 + 3 * SIZE)
    assert all(raw[i * (1 + 3 * SIZE)] == 0 for i in range(SIZE))
    return raw


def main():
    with tempfile.TemporaryDirectory() as directory:
        a = generate_all(Path(directory) / 'a')
        b = generate_all(Path(directory) / 'b')
        assert set(a) == {'Corrugated', 'Concrete', 'Floor'}
        for kind in a:
            assert a[kind].read_bytes() == b[kind].read_bytes()
            pixels = load_rgb(a[kind])
            assert max(pixels) - min(pixels) > 80
            assert a[kind].stat().st_size > 1000
        print('テクスチャ生成・PNG形式・再現性: 成功')


if __name__ == '__main__':
    main()

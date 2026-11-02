"""外部画像・追加ライブラリなしで、検証用の不規則な表面画像を生成する。"""
import math
import random
import struct
import zlib
from pathlib import Path

SIZE = 256


def _chunk(tag, data):
    return struct.pack('>I', len(data)) + tag + data + struct.pack(
        '>I', zlib.crc32(tag + data) & 0xffffffff)


def _png(path, rows):
    raw = b''.join(b'\x00' + bytes(row) for row in rows)
    data = (b'\x89PNG\r\n\x1a\n'
            + _chunk(b'IHDR', struct.pack('>2I5B', SIZE, SIZE, 8, 2, 0, 0, 0))
            + _chunk(b'IDAT', zlib.compress(raw, 7))
            + _chunk(b'IEND', b''))
    path.write_bytes(data)


def _smooth(values, x, y):
    """低周波汚れを補間する。単調な格子模様を防ぐ。"""
    width = len(values)
    u, v = x % width, y % width
    a, b = int(u), int(v)
    fx, fy = u - a, v - b
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    x0, x1 = a % width, (a + 1) % width
    y0, y1 = b % width, (b + 1) % width
    return ((values[y0][x0] * (1 - fx) + values[y0][x1] * fx) * (1 - fy)
            + (values[y1][x0] * (1 - fx) + values[y1][x1] * fx) * fy)


def _surface(kind, seed):
    rng = random.Random(seed)
    coarse = [[rng.uniform(-1, 1) for _ in range(16)] for _ in range(16)]
    fine = [[rng.uniform(-1, 1) for _ in range(32)] for _ in range(32)]
    rows = []
    for y in range(SIZE):
        row = bytearray()
        drift = 2.2 * math.sin(y * 0.033 + seed) + 0.9 * math.sin(y * 0.091)
        for x in range(SIZE):
            cloudy = _smooth(coarse, x / 16, y / 16)
            grain = _smooth(fine, x / 8, y / 8)
            speck = rng.uniform(-1, 1)
            if kind == 'Corrugated':
                # 断面の筋に湾曲・汚れ・擦り傷を重ねる。
                rib = math.cos((x + drift + 1.7 * grain) * math.tau / 21)
                value = 205 + 25 * rib + 19 * cloudy + 8 * grain + 6 * speck
                if rng.random() < 0.0025:
                    value -= rng.uniform(35, 90)
            elif kind == 'Floor':
                value = 202 + 30 * cloudy + 12 * grain + 11 * speck
                # 断続的な塗装の擦れ
                if ((x + y * 3) % 113 < 2 and grain > 0.15):
                    value -= 37
            else:
                value = 206 + 42 * cloudy + 15 * grain + 17 * speck
                if rng.random() < 0.004:
                    value -= 48
            v = max(25, min(255, int(value)))
            if kind == 'Floor':
                rgb = (v, max(0, v - 5), max(0, v - 12))
            elif kind == 'Corrugated':
                rgb = (v, max(0, v - 3), max(0, v - 6))
            else:
                rgb = (v, v, max(0, v - 2))
            row.extend(rgb)
        rows.append(row)
    return rows


def generate_all(directory):
    """PNG生成先を返す。乱数固定・毎回同一の画像になる。"""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    generated = {}
    for index, name in enumerate(('Corrugated', 'Concrete', 'Floor')):
        path = directory / ('T_' + name + '.png')
        _png(path, _surface(name, 20261102 + index))
        generated[name] = path
    return generated


if __name__ == '__main__':
    generate_all(Path(__file__).resolve().parents[1] / 'Saved' / 'GeneratedTextures')

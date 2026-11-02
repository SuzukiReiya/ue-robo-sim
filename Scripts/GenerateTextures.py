"""2048pxのBaseColor・Normal・Roughnessを固定seedで生成する（外部依存なし）。"""

from array import array
import json
import math
from pathlib import Path
import random
import struct
import zlib

SIZE = 2048
VERSION = "pbr-v2"
SEED = 20261103
SURFACES = ("Corrugated", "Concrete", "Floor")
MAPS = ("BaseColor", "Normal", "Roughness")
_HEADER = b"\x89PNG\r\n\x1a\n"


def _chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))


def _write_png(path, size, rows):
    """1行ずつ可逆圧縮して書き込む。フィルタ0のRGB8 PNG。"""
    encoder = zlib.compressobj(level=3)
    compressed = bytearray()
    count = 0
    for row in rows:
        if len(row) != 3 * size:
            raise ValueError("行のピクセル数が不正です")
        compressed.extend(encoder.compress(b"\x00" + row))
        count += 1
    if count != size:
        raise ValueError("画像の行数が不正です")
    compressed.extend(encoder.flush())
    payload = (_HEADER
               + _chunk(b"IHDR", struct.pack(">2I5B", size, size, 8, 2, 0, 0, 0))
               + _chunk(b"IDAT", bytes(compressed))
               + _chunk(b"IEND", b""))
    temp = path.with_suffix(".tmp")
    temp.write_bytes(payload)
    temp.replace(path)


def _clamp8(value):
    return max(0, min(255, int(value + 0.5)))


def _noise_grid(seed, size):
    rng = random.Random(seed)
    return [[rng.uniform(-1.0, 1.0) for _ in range(size)] for _ in range(size)]


def _coordinates(size, grid_size):
    result = []
    for x in range(size):
        at = x * grid_size / size
        index = int(at)
        fraction = at - index
        weight = fraction * fraction * (3.0 - 2.0 * fraction)
        result.append((index, (index + 1) % grid_size, weight))
    return result


def _surface_maps(name, size):
    """高さ場を共有して色と法線が矛盾しないように生成する。"""
    seed = SEED + SURFACES.index(name) * 181
    rng = random.Random(seed)
    coarse = _noise_grid(seed, 16)
    fine = _noise_grid(seed + 1, 96)
    coarse_xy = _coordinates(size, 16)
    fine_xy = _coordinates(size, 96)
    heights = array("f", [0.0]) * (size * size)
    colors = bytearray(size * size * 3)
    roughness = bytearray(size * size * 3)
    # UVの境界をまたいで模様が繋がるよう、リブと低周波ノイズは周期化。
    wave_x = [math.sin(x * 2.0 * math.pi * 3 / size) for x in range(size)]
    rib_x = [x * 2.0 * math.pi * 128 / size for x in range(size)]
    wave_y = [math.sin(y * 2.0 * math.pi * 5 / size) for y in range(size)]
    for y in range(size):
        cy0, cy1, fy = coarse_xy[y]
        fy0, fy1, fyw = fine_xy[y]
        cr0, cr1 = coarse[cy0], coarse[cy1]
        fr0, fr1 = fine[fy0], fine[fy1]
        drift = 0.20 * wave_y[y]
        y_offset = y * size
        for x in range(size):
            cx0, cx1, fx = coarse_xy[x]
            nx0, nx1, nx = fine_xy[x]
            # 格子ノイズを線形補間し、単純な繰り返し線を目立たなくする。
            low = ((cr0[cx0] * (1 - fx) + cr0[cx1] * fx) * (1 - fy)
                   + (cr1[cx0] * (1 - fx) + cr1[cx1] * fx) * fy)
            high = ((fr0[nx0] * (1 - nx) + fr0[nx1] * nx) * (1 - fyw)
                    + (fr1[nx0] * (1 - nx) + fr1[nx1] * nx) * fyw)
            grit = rng.uniform(-1.0, 1.0)
            scratch = rng.random() < 0.0015
            if name == "Corrugated":
                rib = math.cos(rib_x[x] + drift + 0.12 * high)
                height = 0.50 * rib + 0.16 * low + 0.06 * high + 0.020 * grit
                base = 215 + 12 * rib + 16 * low + 7 * high + 4 * grit
                r = 181 + 14 * high + 8 * low
            elif name == "Floor":
                height = 0.08 * low + 0.06 * high + 0.012 * grit
                base = 192 + 22 * low + 8 * high + 5 * grit
                r = 206 + 21 * low + 9 * high
                if scratch and high > 0:
                    base -= 48
            else:
                height = 0.12 * low + 0.09 * high + 0.026 * grit
                base = 196 + 28 * low + 14 * high + 7 * grit
                r = 229 + 12 * low + 7 * high
            if scratch:
                base -= 18
                r += 7
            offset = y_offset + x
            heights[offset] = height
            color_offset = offset * 3
            v = _clamp8(base)
            if name == "Floor":
                colors[color_offset:color_offset + 3] = bytes(
                    (v, _clamp8(v - 6), _clamp8(v - 10)))
            elif name == "Corrugated":
                colors[color_offset:color_offset + 3] = bytes(
                    (v, _clamp8(v - 3), _clamp8(v - 8)))
            else:
                colors[color_offset:color_offset + 3] = bytes(
                    (v, v, _clamp8(v - 2)))
            q = _clamp8(r)
            roughness[color_offset:color_offset + 3] = bytes((q, q, q))

    def byte_rows(data):
        stride = size * 3
        for y in range(size):
            yield data[y * stride:(y + 1) * stride]

    def normal_rows():
        # 高さ場の周期境界で法線を求める。青=上向き、緑=UE DirectX系。
        strength = 1.6 if name == "Corrugated" else 3.0
        for y in range(size):
            yp = (y + 1) % size
            ym = (y - 1) % size
            row = bytearray(size * 3)
            for x in range(size):
                xp = (x + 1) % size
                xm = (x - 1) % size
                dx = (heights[y * size + xm] - heights[y * size + xp]) * strength
                dy = (heights[ym * size + x] - heights[yp * size + x]) * strength
                inv = 1.0 / math.sqrt(dx * dx + dy * dy + 1.0)
                ix = x * 3
                row[ix] = _clamp8((dx * inv * 0.5 + 0.5) * 255)
                row[ix + 1] = _clamp8((dy * inv * 0.5 + 0.5) * 255)
                row[ix + 2] = _clamp8((inv * 0.5 + 0.5) * 255)
            yield row

    return {"BaseColor": byte_rows(colors),
            "Normal": normal_rows(),
            "Roughness": byte_rows(roughness)}


def _valid_png(path, size):
    if not path.is_file():
        return False
    with path.open("rb") as handle:
        header = handle.read(24)
    return (len(header) == 24 and header[:8] == _HEADER
            and header[12:16] == b"IHDR"
            and struct.unpack(">II", header[16:24]) == (size, size))


def generate_all(directory, size=SIZE):
    """生成ファイルのパスを返す。完全な出力があれば再生成しない。"""
    if not 32 <= size <= SIZE:
        raise ValueError("テクスチャ解像度は32～2048で指定してください")
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    manifest = directory / ("manifest_" + VERSION + "_" + str(size) + ".json")
    outputs = {
        name: {kind: directory / f"T_{name}_{kind}_{VERSION}_{size}.png"
               for kind in MAPS}
        for name in SURFACES
    }
    expected = {"version": VERSION, "seed": SEED, "size": size,
                "surfaces": list(SURFACES), "maps": list(MAPS)}
    try:
        cached = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        cached = None
    if cached != expected or any(not _valid_png(p, size)
                                 for group in outputs.values() for p in group.values()):
        for name, paths in outputs.items():
            data = _surface_maps(name, size)
            for kind, path in paths.items():
                _write_png(path, size, data[kind])
        # 必要なファイルが揃ってからキャッシュ情報を公開する。
        manifest.write_text(json.dumps(expected, ensure_ascii=False, indent=2),
                            encoding="utf-8")
    return outputs


if __name__ == "__main__":
    generate_all(Path(__file__).resolve().parents[1] / "Saved" / "GeneratedTextures")

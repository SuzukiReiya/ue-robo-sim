"""ローカルの未遅延カメラ画像で赤い対象を追尾する最小比較器。正解座標は使用しない。"""
import argparse
import json
import socket
import time
from pathlib import Path


def read_frame(directory):
    """公開前後のマニフェストを照合し、書き換え中のフレームを棄却する。"""
    path = directory / 'latest.json'
    before = path.read_bytes()
    manifest = json.loads(before)
    name = manifest['image']
    if name not in ('frame_0.png', 'frame_1.png'):
        raise ValueError('想定外の画像名です')
    data = (directory / name).read_bytes()
    if path.read_bytes() != before:
        return None
    return manifest, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sensor', type=Path, default=Path('Saved/Sensor'))
    args = parser.parse_args()
    import cv2
    import numpy as np
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    last_frame = None
    last_new = time.monotonic()
    def send(turn):
        # ミリ秒epochはJSONの整数精度内。50Hzなので同値の連送を避けられる。
        command = dict(seq=time.time_ns() // 1_000_000, throttle=0.0, turn=float(turn))
        sock.sendto(json.dumps(command).encode(), ('127.0.0.1', 9876))
    print('追尾を開始しました。UEで2（共有）または3（自動）を選択してください。Ctrl+Cで停止。')
    try:
        while True:
            try:
                frame = read_frame(args.sensor)
                if frame:
                    meta, data = frame
                    stamp = (meta['id'], meta['capture_sim_s'])
                    if stamp != last_frame:
                        last_frame = stamp
                        last_new = time.monotonic()
                        image = cv2.imdecode(np.frombuffer(data, dtype=np.uint8), cv2.IMREAD_COLOR)
                        if image is None:
                            raise ValueError('画像を復号できませんでした')
                        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
                        mask = cv2.inRange(hsv, (0, 100, 60), (10, 255, 255)) | cv2.inRange(hsv, (170, 100, 60), (179, 255, 255))
                        count, _, stats, centers = cv2.connectedComponentsWithStats(mask)
                        candidates = [i for i in range(1, count) if stats[i, cv2.CC_STAT_AREA] >= 20]
                        turn = 0.0
                        if candidates:
                            best = max(candidates, key=lambda i: stats[i, cv2.CC_STAT_AREA])
                            error = (centers[best][0] - image.shape[1] / 2) / (image.shape[1] / 2)
                            turn = max(-1.0, min(1.0, error * 0.8))
                        send(turn)
                if time.monotonic() - last_new > 0.2:
                    send(0.0)
            except (OSError, ValueError, KeyError, cv2.error):
                send(0.0)
            time.sleep(0.02)
    except KeyboardInterrupt:
        pass
    finally:
        send(0.0)
        sock.close()

if __name__ == '__main__':
    main()

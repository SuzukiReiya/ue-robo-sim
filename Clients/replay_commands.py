"""記録した適用指令を外部指令として再送する。物理状態の完全再現ではない。"""
import argparse
import csv
import json
import socket
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    args = parser.parse_args()
    with args.csv.open(encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise SystemExit('記録が空です')
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    first = float(rows[0]['sim_s'])
    start = time.monotonic()
    def send(throttle, turn):
        payload = dict(seq=time.time_ns() // 1_000_000, throttle=throttle, turn=turn)
        sock.sendto(json.dumps(payload).encode(), ('127.0.0.1', 9876))
    print('UEを再起動して3（自動）に切り替えた状態で使用してください。')
    try:
        for row in rows:
            target = start + float(row['sim_s']) - first
            while time.monotonic() < target:
                time.sleep(min(0.01, max(0, target - time.monotonic())))
            send(float(row['applied_throttle']), float(row['applied_turn']))
    except KeyboardInterrupt:
        pass
    finally:
        send(0.0, 0.0)
        sock.close()

if __name__ == '__main__':
    main()

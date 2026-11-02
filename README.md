# ue-robo-sim

Unreal Engine 5.6を使う、遅延・共有制御・自動制御の検証用ロボットシミュレータです。
AP5の「C++でシーンを生成し、PowerShellでビルド・マップ生成・起動する」構成を基に、独立したプロジェクトとして作成しました。破壊・変形機能は含みません。

**現在は初期実装です。UEに依存しない制御基盤はテスト済みですが、Windows / UEでのビルド・描画・操作は未確認です。実機相当の精度やリアルタイム性能は保証しません。**

## 初期版の機能

- 20 m四方の簡易フィールド、自機、赤い移動対象、障害物をC++で生成。
- WASD操作と一人称カメラ映像。操縦者画面は遅延させた画像だけを表示。
- 指令遅延と映像遅延を独立設定。揺らぎ・欠落率・乱数seedも設定可能。
- 手動／共有／外部自動制御。共有は手動と外部指令の重み付き合成。
- 120 Hzの運動更新、最大1.5 m/s・120度/s、時定数150 msの一次遅れ。
- 最大20 fps、640×360、画角90度のカメラ。描画FPSが低いと取得FPSも低下。
- 機体側カメラ画像をPNGで外部出力（任意）、localhost UDPで外部指令受信。
- 実験設定と機体真値・各指令のCSV記録。記録した指令の再送サンプル。
- 正解座標を使わず赤い画像領域から向きを補正するPythonサンプル。

`main`は競技非依存の基盤とします。競技固有の寸法・ルール・モデルは、後から `feature/attracts` などの専用ブランチに追加する方針です。現状の寸法は競技公式値ではありません。

## 必要環境

- Windows 11、Windows PowerShell 5.1またはPowerShell 7。
- Unreal Engine 5.6.x。AP5用に導入済みのUEを利用できます。
- Visual Studio 2022のC++ビルドツール、対応するWindows SDK。
- Python 3.10以降は外部サンプル利用時のみ必要。

UE本体はEpic Games Launcherから導入してください。ライセンスの同意・アカウント登録は自動化しません。

## ビルド・起動

```powershell
git clone https://github.com/SuzukiReiya/ue-robo-sim.git
cd ue-robo-sim
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\BuildAndRun.ps1 -EngineRoot "D:\Epic Games\UE_5.6"
```

実際のUE導入先を指定してください。省略時は環境変数`ROBOT_SIM_UE_ROOT`、続いて標準パス`C:\Program Files\Epic Games\UE_5.6`を使用します。
起動方式は開発用のStandalone Gameです。配布用exeのパッケージ化はまだ用意していません。
マップと材質を初回生成するため、手動のエディタ配置は不要です。

```powershell
# 遅延なしの基準条件
.\Scripts\BuildAndRun.ps1 -CommandDelayMs 0 -VideoDelayMs 0
# 指令100ms、映像200ms、揺らぎ±30ms、欠落率2%
.\Scripts\BuildAndRun.ps1 -CommandDelayMs 100 -VideoDelayMs 200 -JitterMs 30 -LossRate 0.02 -Seed 12345
# ビルド・マップ生成のみ
.\Scripts\BuildAndRun.ps1 -BuildOnly
```

起動後、画面をクリックしてキーボードのフォーカスを与えます。

| キー | 動作 |
|---|---|
| W / S | 前進／後退 |
| A / D | 左旋回／右旋回 |
| 1 | 手動 |
| 2 | 共有制御（外部指令との合成） |
| 3 | 外部自動制御 |
| Space長押し | 検証用の遅延なし非常停止 |
| F1 | 研究用の状態表示を切替（初期状態は非表示） |
| Esc | 終了 |

モード切替では指令キューを消去し、新しい入力を待ちます。機体速度は時定数に従って変化します。Spaceのみ速度も即座にゼロにします。
外部指令が250ms（シミュレーション時間）届かなければ、自動モードは停止指令、共有モードは手動に戻ります。手動指令も最終到着から250msでゼロにします。

## 画像を使う外部制御

```powershell
# 端末1: 機体側画像を出力して起動
.\Scripts\BuildAndRun.ps1 -ExportFrames
# 端末2: リポジトリ直下で実行
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r Clients\requirements.txt
.\.venv\Scripts\python.exe Clients\color_tracker.py
```

UEで`2`または`3`を押すと、赤い対象が画面中央へ寄るよう旋回指令を出します。これは画像入出力を確認する色抽出の比較器であり、学習済み追跡モデルではありません。自動モードでは前進しません。共有モードの既定重みは0.5で、外部の前進指令がゼロなので手動前進も半分になります。`-SharedWeight`で変更できます。

画像出力は操縦者向け映像遅延を適用する前の画像です。機体内処理を想定しています。PC遠隔処理の画像転送遅延や実機の通信は未実装です。

## 記録・再実行

`Saved/Experiments/<実行ID>/settings.json`と`truth.csv`へ設定と記録を保存します。真値CSVは評価者専用で、追尾サンプルは読みません。
同じ設定・seedで条件を再実行できますが、入力タイミング・描画・OSスケジューリングまで決定的にはなりません。

```powershell
# UEを再起動して3（自動）へ切替後、過去の適用指令を送る
python Clients\replay_commands.py Saved\Experiments\<実行ID>\truth.csv
```

この再送は適用指令を壁時計に従って送り直す簡易方式です。元の通信遅延を再適用せず、相手の位相や機体軌跡の完全一致も保証しません。録画再生・状態スナップショット・厳密な決定的リプレイは今後の実装です。

## 検証と制約

```bash
g++ -std=c++17 -Wall -Wextra -Werror Tests/test_core.cpp -o /tmp/test-core
/tmp/test-core
python3 -m py_compile Clients/*.py Scripts/CreateMap.py
```

GitHub ActionsはC++基盤テストとPowerShell構文確認を行います。UE本体のビルドは含みません。AP5のself-hosted runnerがリポジトリ単位で登録されている場合、このリポジトリからそのまま利用できるとは限りません。

- ロボットの車輪接触・滑り・坂道・転倒は未実装。平面の運動学モデルです。
- 並進はSweepで接触を扱いますが、旋回の形状Sweepは未実装。精密な接触評価には使えません。
- カメラの露出・材質は仮設定。フォトリアルなモデル、圧縮劣化、レンズ歪み、実機との校正は今後対応します。
- PNG出力は同期GPU readbackを行うため、フレーム時間を増加させます。高速な画像認識の性能評価には、非同期readback・共有メモリ等への置換が必要です。
- 設定した遅延にフレーム量子化・描画・readbackの時間が加わります。遅延0は物理的な入出力遅延ゼロを意味しません。
- 1フレーム250msを超える停止では超過時間を破棄し、`SIM_OVERRUN`を出します。その試行は実時間の遅延比較から除外してください。
- 画像保存は最新2枚のみ。全フレームの収録・認識結果の記録・自動集計は未実装です。
- カメラ停止時は最終画像を保持します。研究表示で画像経過時間を確認できます。

起動ログは`.local/logs/`、UEログは`Saved/Logs/`です。
初回Windows検証の手順は[検証チェックリスト](docs/検証チェックリスト.md)、通信仕様は[外部接続仕様](docs/外部接続仕様.md)を参照してください。

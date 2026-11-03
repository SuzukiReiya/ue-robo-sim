# 開発方針

- コードの説明、文書、コミットメッセージは日本語を基本とする。
- UE 5.6.xを基準とし、Windowsで未検証のものを検証済みと扱わない。
- mainは競技非依存の基盤とする。競技固有機能は専用ブランチで開発する。
- 制御入力に正解座標を混ぜない。真値は評価ログに分離する。
- 遅延は指令経路と映像経路を分離する。機体内制御には通信往復遅延を追加しない。
- 入力取得（SimKeyboardInput）／割り当て（SimInputBindings）／指令裁定（SimControl）／機体運動（SimVehicle）／射撃（SimWeapon）／撮影・映像遅延（SimVideoPipeline）／場面（SimEnvironment）／UI（SimSettingsWidget）の境界を維持する。
- Esc一時停止はシミュレーション時計を止めて実現し、Slate/UMGの入力処理は動作させる。再開時は古い入力・UDPを流用しない。
- 制御判断からSceneCaptureやHUD、機体Actor位置を直接変更しない。機体と映像は制御モードを知らない構造に保つ。
- 変更時はTests/test_core.cpp、Tests/test_textures.pyとPythonの構文を確認する。
- 外部通信は初期状態でlocalhostに限定する。

# DialDeck

丸型ノブディスプレイ Guition JC3636K718C（ESP32-S3）を、Mac 用の BLE 入力デバイスにするファームウェア。

- Stream Deck のように、画面のボタンでショートカットを送る
- ノブでスクロールし、画面をタッチパッドにしてマウスとして使う
- herdr と連携し、エージェントの状態表示や操作をする

## 方針

- 接続は BLE のみ。Wi-Fi は使わない
- BLE HID: キーボードショートカット、マウス、スクロール、メディアキー
- BLE 独自 GATT: Mac 側常駐プログラム（Swift / CoreBluetooth）と双方向通信し、herdr と連携する。時刻も Mac から送る
- USB: 書き込み、ログ、充電。OTA は使わない
- ファームウェアは ESP-IDF（NimBLE、LVGL、`esp_lcd_st77916`）

## セットアップ

リポジトリの取得から、開発環境の構築、ファームのビルドと書き込み、Mac アプリのインストールまでは [docs/setup.md](docs/setup.md) を参照する。

## ハードウェア

- ESP32-S3（rev v0.2）、PSRAM 8MB、フラッシュ 16MB
- 1.8" 円形 360×360（ST77916, QSPI）、タッチ CST816、ロータリーノブ（押し込み不可）、LED リング（GPIO0）、スピーカー、マイク、microSD、バッテリー

ピン配置と既知の注意点は `.claude/skills/guition-jc3636k718c` を参照する。出典リポジトリの取得と、そこから取り込んだものは [references/README.md](references/README.md)。実機で確かめたことと、上流と違った点は [docs/hardware-notes.md](docs/hardware-notes.md) にある。

## 純正ファームウェアのバックアップ

DialDeck を書き込むと純正ファームは消える。初めて書き込む前に、[docs/stock-firmware-backup.md](docs/stock-firmware-backup.md) の手順でフラッシュ全体を `backup/` に吸い出しておく。書き戻し手順も同じ文書にある。

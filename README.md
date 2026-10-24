# DialDeck

丸型ノブディスプレイ Guition JC3636K718C（ESP32-S3）を、Mac 用の BLE 入力デバイスにする。

- Stream Deck のように、画面のボタンでショートカットを送る
- ノブでスクロールし、画面をタッチパッドにしてマウスとして使う
- herdr と連携し、エージェントの状態表示や操作をする

動くものは二つ。デバイスのファームウェアと、Mac のウィンドウアプリ。ファイルの対応は [docs/architecture.md](docs/architecture.md)。

## 技術スタック

| | 使うもの | 置いてある場所 |
| --- | --- | --- |
| ファームウェア | ESP-IDF v6.1、NimBLE、LVGL 9、`esp_lcd_st77916` | `firmware/`。アプリは `firmware/main` |
| Mac アプリ | Swift 6.0 の Package、macOS 14、CoreBluetooth、SwiftUI | `mac/`。Xcode のプロジェクトは無い |
| デバイスとの接続 | BLE HID（キーボード、マウス、メディアキー）と、独自 GATT | [docs/config-protocol.md](docs/config-protocol.md) |
| herdr | Mac アプリだけがソケットを開く。デバイスは GATT 経由で操作を頼む | `mac/Sources/DialDeckKit/HerdrBridge.swift` |
| エディタ | Cursor。C は Apple clangd、Swift は SourceKit-LSP | [docs/editor.md](docs/editor.md) |

無線は BLE だけ。Wi-Fi と OTA は使わない。USB は書き込み、ログ、充電。

## ドキュメント

| 文書 | 内容 |
| --- | --- |
| [docs/setup.md](docs/setup.md) | リポジトリの取得、ESP-IDF、ビルド、書き込み、Mac アプリ |
| [docs/architecture.md](docs/architecture.md) | ファームと Mac アプリの対応、BLE で何が通るか |
| [docs/editor.md](docs/editor.md) | Cursor に何を入れ、どの警告を見るか |
| [docs/config-protocol.md](docs/config-protocol.md) | GATT のバイト列 |
| [docs/hardware-notes.md](docs/hardware-notes.md) | 実機で確かめたこと |
| [docs/stock-firmware-backup.md](docs/stock-firmware-backup.md) | 純正ファームの吸い出しと書き戻し |
| [references/README.md](references/README.md) | 出典リポジトリ。ビルドには使わない |
| [AGENTS.md](AGENTS.md) | コードを触るときに外さないこと |

## ハードウェア

- ESP32-S3（rev v0.2）、PSRAM 8MB、フラッシュ 16MB
- 1.8" 円形 360×360（ST77916, QSPI）、タッチ CST816、ロータリーノブ（押し込み不可）、LED リング（GPIO0）、スピーカー、マイク、microSD、バッテリー

ピン配置と既知の注意点は `.claude/skills/guition-jc3636k718c` を参照する。出典リポジトリの取得と、そこから取り込んだものは [references/README.md](references/README.md)。実機で確かめたことと、上流と違った点は [docs/hardware-notes.md](docs/hardware-notes.md) にある。

## 純正ファームウェアのバックアップ

DialDeck を書き込むと純正ファームは消える。初めて書き込む前に、[docs/stock-firmware-backup.md](docs/stock-firmware-backup.md) の手順でフラッシュ全体を `backup/` に吸い出しておく。書き戻し手順も同じ文書にある。

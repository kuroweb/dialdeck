# DialDeck

BLE macro pad / dial mouse for Guition JC3636K718C (ESP32-S3), with herdr integration.

## 方針

- 接続は BLE のみ。Wi-Fi は使わない
- BLE HID: キーボードショートカット、マウス、スクロール、メディアキー
- BLE 独自 GATT: Mac 側常駐プログラム（Swift / CoreBluetooth）と双方向通信し、herdr と連携する。時刻も Mac から送る
- USB: 書き込み、ログ、充電。OTA は使わない
- ファームウェアは ESP-IDF（NimBLE、LVGL、`esp_lcd_st77916`）

## セットアップ

開発環境の構築手順は [docs/setup.md](docs/setup.md) を参照する。

## ハードウェア

- ESP32-S3（rev v0.2）、PSRAM 8MB、フラッシュ 16MB
- 1.8" 円形 360×360（ST77916, QSPI）、タッチ CST816、ロータリーノブ（押し込み不可）、LED リング（GPIO0）、スピーカー、マイク、microSD、バッテリー

ピン配置と既知の注意点は `vendor/esphome-guition-jc3636k718c-va`（参照専用の submodule）と、そのスキル `.claude/skills/guition-jc3636k718c` を参照する。

## 純正ファームウェア

`backup/` に純正ファームのフラッシュ全体を置く（git 管理外）。

- `jc3636k718c_stock_44bd8d6f1fb8.bin`（16MB）
- SHA-256: `73b4181484af66aa56d5650e18d4537dbb69a81ac6039b38d524f26048c3c790`

ダウンロードモード（USB-C ポート左の BOOT を押しながら電源投入）で書き戻す:

```
esptool -p /dev/cu.usbmodem* write-flash 0 backup/jc3636k718c_stock_44bd8d6f1fb8.bin
```

# 開発フロー

日々の変更、ビルド、実機への書き込みに使う手順。環境の初回セットアップは [setup.md](setup.md)、構成は [architecture.md](architecture.md) を参照する。

## 作業を始める

変更する領域に合わせて資料を確認する。

| 作業 | 先に見る資料 |
| --- | --- |
| ピン、液晶、ノブ、タッチ、電池 | [hardware-notes.md](hardware-notes.md) |
| BLE GATT | [config-protocol.md](config-protocol.md) |
| ファイルと通信の構成 | [architecture.md](architecture.md) |
| エディタの診断 | [editor.md](editor.md) |

作業前後の変更は `git status --short` で確認する。コミットは依頼されたときに行う。

## ファームウェア

ESP-IDF を有効にしてビルドする。

```sh
source ~/.espressif/tools/activate_idf_v6.1.sh
cd firmware
idf.py build
```

実機へ書き込むときは USB シリアルのポートを確認し、`flash` だけを実行する。

```sh
ls /dev/cu.usbmodem*
idf.py -p /dev/cu.usbmodemXXXX flash
```

`monitor` は付けない。初めて DialDeck を書き込む前に、[純正ファームウェアのバックアップ](stock-firmware-backup.md)を取る。

## Mac アプリ

ソースをビルドする。Xcode のプロジェクトは使わない。

```sh
cd mac
./scripts/build-app.sh
```

`mac/build/DialDeck.app` ができる。インストールする場合は `--install` を付ける。起動中のアプリを終了して `/Applications/DialDeck.app` を置き換える。

```sh
cd mac
./scripts/build-app.sh --install
```

## 変更箇所ごとの確認

| 変更 | ビルド・確認 |
| --- | --- |
| ファームウェアだけ | `cd firmware && idf.py build`。実機の動作確認には `flash` する |
| Mac アプリだけ | `cd mac && ./scripts/build-app.sh`。実機連携を確認する場合は `--install` する |
| GATT のバイト列 | ファームウェアと Mac アプリの両方をビルドし、[config-protocol.md](config-protocol.md)も更新する |

GATT の互換性がなくなる変更では、ファームウェアの `PROTOCOL_VERSION` と Mac 側の対応する protocol version を同時に上げ、文書に履歴を残す。

## 配布用アプリ

`mac/dist/DialDeck.app` は git 管理するビルド成果物。明示的に更新するときだけ、ソースをコミットした後に生成し、別のコミットにする。

```sh
cd mac
./scripts/build-app.sh --dist
```

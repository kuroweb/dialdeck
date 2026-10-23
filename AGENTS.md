# AGENTS.md

DialDeck の作業メモ。何を作っているかは [README.md](README.md)、手順は [docs/](docs/)。ここには、コードを触るときに外してはいけないことを書く。

## 構成

- `firmware/` は ESP-IDF。アプリは `firmware/main/`。画面は `screen_def_t`（`screen.h`）で、切り替えは `ui.c`。
- `mac/` は Swift Package。`DialDeckKit` が BLE と herdr、`DialDeck` がメニューバーアプリ。ビルドは `mac/scripts/build-app.sh`。Xcode のプロジェクトは無い。
- `docs/` は日本語の手順、実機で確かめたこと、GATT の形式。
- `.claude/skills/guition-jc3636k718c` は上流 v2.2.12 のスキルをそのまま置いたもの。DialDeck 側の差分は書かない。
- `references/` は出典の clone 先。中身は git に入れない。対応は [references/README.md](references/README.md)。
- `backup/` は純正ファームの吸い出し。nvs に認証情報が入り得る。git に入れない。

## 触る前に読む

- ピン、液晶、ノブ、タッチ、バッテリーは `firmware/main/board.h`、スキル、[docs/hardware-notes.md](docs/hardware-notes.md)。スキルは ESPHome 向けの上流の知見で、こちらの実機と違ったことは hardware-notes に書く。
- GATT は [docs/config-protocol.md](docs/config-protocol.md)。ファームは `firmware/main/config_service.c`、Mac は `mac/Sources/DialDeckKit/DeviceLink.swift`。
- セットアップと書き込みは [docs/setup.md](docs/setup.md)。初めて書き込む前のバックアップは [docs/stock-firmware-backup.md](docs/stock-firmware-backup.md)。

## 守ること

- 無線は BLE ペリフェラルだけにする。Wi-Fi と OTA は入れない。USB は書き込み、ログ、充電。
- デバイスから herdr には触れない。herdr のソケットを使うのは Mac アプリだけ。Mac アプリは BLE をスキャンせず、ペアリング済みで接続中の DialDeck を使う。
- ピン配置と `firmware/main/lcd_init_cmds.h` を、出典を確認せずに変えない。初期化列は v2.2.12 の `base/core.yaml` から起こしたもの。
- 画面を足すときは `screen_def_t` を作り、`screen.h` に宣言を追加し、`ui.h` の enum、`ui.c` の表、`firmware/main/CMakeLists.txt` の `SRCS` に載せる。見た目は `theme.h` と `widgets.h` を使う。LVGL を触る関数は `lvgl_port_lock` を持って呼ぶ。
- GATT のバイト列を変えたら、ファーム、Mac、`docs/config-protocol.md` を同じ変更で直す。互換の無い変更はプロトコル版（`PROTOCOL_VERSION` と `supportedProtocol`）を上げ、版の履歴に書く。
- HID の report map を変えると、ペアリング済みの Mac は古い構成を覚えたままになる。再ペアリングが要ることを [docs/hardware-notes.md](docs/hardware-notes.md) に沿って残す。
- `CONFIG_BT_NIMBLE_MAX_CCCDS`、`CONFIG_BT_NIMBLE_MAX_BONDS`、`CONFIG_ESP_MAIN_TASK_STACK_SIZE` を、hardware-notes を読まずに下げない。
- `firmware/main/fonts/font_ja_16.c` は `firmware/tools/make-font.sh` の生成物。手では直さない。
- ドキュメントは日本語で、今の docs と同じく短い事実だけを書く。コードのコメントは英語のまま。
- コミットは頼まれたときだけ。メッセージは Conventional Commits、日本語、命令形。`backup/` と `references/` の clone は入れない。

## 確認

- ファームは ESP-IDF を有効にして `cd firmware && idf.py build`。
- Mac アプリは `cd mac && ./scripts/build-app.sh`。`/Applications` へ置くときは `--install`。署名は ad hoc。
- `idf.py flash` は頼まれたときだけ。書き込むと純正ファームは消える。
- 実機で確かめていないことを、確かめたこととして docs に書かない。

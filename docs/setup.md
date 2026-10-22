# 開発環境のセットアップ（macOS）

リポジトリの取得から、ファームウェアの書き込みと Mac アプリのインストールまで。

ESP-IDF は公式インストーラー EIM（ESP-IDF Installation Manager）で入れる。EIM 自体は Homebrew の tap から入れる。ESP-IDF 本体とツールチェーンは、EIM が `~/.espressif` 配下に置く。

## 1. リポジトリを取得する

ハードウェア情報の参照元として、上流リポジトリ（[esphome-guition-jc3636k718c-va](https://github.com/MichalZaniewicz/esphome-guition-jc3636k718c-va)）を submodule で `vendor/` に入れている。submodule ごと取得する。

```
git clone --recurse-submodules git@github.com:kuroweb/dialdeck.git
cd dialdeck
```

`--recurse-submodules` を付けずに clone した場合は、後から取得する。

```
git submodule update --init
```

`.claude/skills/guition-jc3636k718c` は submodule 内のスキルへのシンボリックリンクなので、submodule が無いと中身が空になる。

## 2. Python を用意する

EIM は Python 3.10〜3.13 を要求する。macOS 標準の 3.9 では `Python sanity check failed` で止まる。pyenv で 3.13 を入れる。

```
pyenv install 3.13.16
```

ESP-IDF はインストール時に専用の venv（`~/.espressif/tools/python/v6.1/venv`）を作るので、3.13 が必要なのはインストールの間だけ。普段使う Python は変えなくてよい。

## 3. EIM を入れる

```
brew tap espressif/eim
brew trust --formula espressif/eim/eim
brew install eim
eim --version
```

`brew trust` は tap 全体ではなく `eim` の formula だけを信頼する。

## 4. ESP-IDF を入れる

```
PYENV_VERSION=3.13.16 eim install -i v6.1 -t esp32s3 -n true
```

- バージョン: v6.1（最新の安定版）
- 対象チップ: esp32s3 のみ

## 5. ESP-IDF を有効にする

ターミナルを開くたびに読み込む。

```
source ~/.espressif/tools/activate_idf_v6.1.sh
idf.py --version
```

## 6. esptool を入れる

純正ファームのバックアップや書き戻しに使う。ESP-IDF にも同梱されるが、ESP-IDF を有効にしていないターミナルでも使えるよう単体で入れておく。

```
pipx install esptool
```

## 7. ビルドする

```
cd firmware
idf.py build
```

初回は依存コンポーネント（LVGL、ST77916 ドライバなど）を `firmware/managed_components/` に取得する。

## 8. 書き込む

初めて書き込むと純正ファームが消える。先に [純正ファームウェアのバックアップ](stock-firmware-backup.md) を取る。

```
idf.py -p /dev/cu.usbmodem* flash monitor
```

`monitor` を抜けるには `Ctrl+]`。

## 9. Mac アプリを入れる

ボタン画面の割り当ては Mac アプリで変える。Swift Package Manager でビルドするので、`swift` コマンドが使えればよい（Xcode のプロジェクトは使わない）。

```
cd mac
./scripts/build-app.sh --install
```

`mac/build/DialDeck.app` を作り、`/Applications/DialDeck.app` にコピーする。起動中の DialDeck は終了してから置き換える。`--install` を付けなければ `mac/build/` に作るだけ。

アプリは ad hoc 署名なので、ビルドした Mac でだけ開ける。

### 初めて起動したとき

1. `/Applications/DialDeck.app` を開く
2. Bluetooth の利用を求めるダイアログで許可する。ビルドし直すと署名が変わるので、もう一度聞かれることがある
3. DialDeck を Mac とペアリングしておく（システム設定 → Bluetooth）。アプリはペアリング済みで接続中の DialDeck を使い、自分では探さない

ウィンドウに `Not connected` と出たままなら、ファームの GATT サービスが変わって Mac が古い構成を覚えている可能性がある。システム設定 → Bluetooth で DialDeck を削除し、ペアリングし直す。

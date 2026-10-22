# 開発環境のセットアップ（macOS）

ESP-IDF は公式インストーラー EIM（ESP-IDF Installation Manager）で入れる。EIM は Homebrew の tap から入れ、ESP-IDF 本体とツールチェーンは EIM が `~/.espressif` 配下に入れる。

## 1. Python を用意する

EIM は Python 3.10〜3.13 を要求する。macOS 標準の 3.9 では `Python sanity check failed` で止まる。pyenv で 3.13 を入れる。

```
pyenv install 3.13.16
```

ESP-IDF はインストール時に専用の venv（`~/.espressif/tools/python/v6.1/venv`）を作るので、3.13 が必要なのはインストールの間だけ。普段使う Python は変えなくてよい。

## 2. EIM を入れる

```
brew tap espressif/eim
brew trust --formula espressif/eim/eim
brew install eim
eim --version
```

`brew trust` は tap 全体ではなく `eim` の formula だけを信頼する。

## 3. ESP-IDF を入れる

```
PYENV_VERSION=3.13.16 eim install -i v6.1 -t esp32s3 -n true
```

- バージョン: v6.1（最新の安定版）
- 対象チップ: esp32s3 のみ

## 4. ESP-IDF を有効にする

ターミナルを開くたびに読み込む。

```
source ~/.espressif/tools/activate_idf_v6.1.sh
idf.py --version
```

## 5. esptool

純正ファームのバックアップや書き戻しに使う。ESP-IDF にも同梱されるが、単体でも入れておく。

```
pipx install esptool
```

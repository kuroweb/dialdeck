# 開発環境のセットアップ（macOS）

リポジトリの取得から、ファームウェアの書き込みと Mac アプリのインストールまで。

日々の開発フローとコマンドは [development.md](development.md) を参照する。

ESP-IDF は公式インストーラー EIM（ESP-IDF Installation Manager）で入れる。EIM 自体は Homebrew の tap から入れる。ESP-IDF 本体とツールチェーンは、EIM が `~/.espressif` 配下に置く。

## 1. リポジトリを取得する

```
git clone git@github.com:kuroweb/dialdeck.git
cd dialdeck
```

ハードウェアの出典は、リポジトリルートの `references/` に clone する。何を取り込んであるかは [references/README.md](../references/README.md)。

```
git clone --branch v2.2.12 https://github.com/MichalZaniewicz/esphome-guition-jc3636k718c-va.git references/esphome-guition-jc3636k718c-va
```

v2.2.12 は annotated tag なので、clone 時に `is not a commit` と出る。その後の checkout 先が `8ffed08013312bf52d72bb2bf78dc208abf8db34` なら、取り込んだ版と同じ。

`.claude/skills/guition-jc3636k718c` はリポジトリ内のファイルなので、この clone が無くても読める。clone は、出典の YAML やピン配置を追うときに使う。clone した中身は git に入れない。

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

初回は依存コンポーネント（LVGL、ST77916 ドライバなど）を `firmware/managed_components/` に取得する。同じ build が `firmware/build/compile_commands.json` を作る。Cursor が C の警告を出すのに要る。入れ方は [editor.md](editor.md)。

## 8. 書き込む

初めて書き込むと純正ファームが消える。先に [純正ファームウェアのバックアップ](stock-firmware-backup.md) を取る。

```
idf.py -p /dev/cu.usbmodem* flash
```

## 9. Mac アプリを入れる

ボタン画面の割り当ては Mac アプリで変える。Swift Package Manager でビルドするので、`swift` コマンドが使えればよい（Xcode のプロジェクトは使わない）。

```
cd mac
./scripts/build-app.sh --install
```

`mac/build/DialDeck.app` を作り、`/Applications/DialDeck.app` にコピーする。起動中の DialDeck は終了してから置き換える。`--install` を付けなければ `mac/build/` に作るだけ。バイナリは Apple シリコンと Intel の両方を含む。

### ビルドせずに入れる

`mac/dist/DialDeck.app` はビルド済みのものを git に入れてある。ビルド環境の無い Mac では、clone してこれをコピーする。

```
cp -R mac/dist/DialDeck.app /Applications/
```

署名は ad hoc。ブラウザで落とした zip などから取り出したものは Gatekeeper に止められるので、`xattr -dr com.apple.quarantine /Applications/DialDeck.app` で隔離属性を外す。

`mac/dist/` を更新するときは、ソースをコミットしてから `./scripts/build-app.sh --dist` を実行し、`mac/dist/` を別にコミットする。アプリの `CFBundleVersion` に元のコミットが入り、未コミットの変更があれば `-dirty` が付く。

### 初めて起動したとき

1. `/Applications/DialDeck.app` を開く。Dock には出ず、メニューバーにダイヤルのアイコンが出る。設定はメニューの Settings… で開く。ログインのたびに起動するなら、メニューの Launch at Login を入れる
2. Bluetooth の利用を求めるダイアログで許可する。ビルドし直すと署名が変わるので、もう一度聞かれることがある
3. DialDeck を Mac とペアリングしておく（システム設定 → Bluetooth）。アプリはペアリング済みで接続中の DialDeck を使い、自分では探さない

設定ウィンドウやメニューに `Not connected` と出たままなら、ファームの GATT サービスが変わって Mac が古い構成を覚えている可能性がある。システム設定 → Bluetooth で DialDeck を削除し、ペアリングし直す。

### Claude の使用量を出す

Claude Code 2.1.80 以降の Pro / Max で使える。最初の API 応答後に使用量が入り、Claude Code を閉じている間は更新されない。仕様は [Claude Code の status line](https://code.claude.com/docs/en/statusline) を参照する。

`jq` を用意し、`~/.claude/settings.json` の `statusLine` コマンドで実行するスクリプトへ次を追加する。既存スクリプトがあれば、入力を読んだ後へ追記する。入力 JSON 全体を変数 `input` に保持している前提（まだ読んでいなければ `input=$(cat)` で読む）。

```sh
rate_limits=$(printf '%s' "$input" | jq -c '.rate_limits // empty')
if [ -n "$rate_limits" ]; then
    usage_dir="$HOME/Library/Application Support/DialDeck"
    mkdir -p "$usage_dir" &&
        usage_tmp=$(mktemp "$usage_dir/claude-rate-limits.json.XXXXXX") &&
        { printf '%s\n' "$rate_limits" > "$usage_tmp" &&
            mv "$usage_tmp" "$usage_dir/claude-rate-limits.json"; }
fi
```

`.rate_limits` オブジェクトだけを保存する。`resets_at` は Unix 秒。同じディレクトリの一時ファイルから `mv` で原子的に置き換える。Mac アプリは毎分読み、期限を過ぎた窓は不明として表示する。キーチェーンは使わない。

# Cursor で開く

リポジトリのルートを開く。`firmware/` や `mac/` だけを開くと、もう一方の言語サーバーがこの設定を読まない。

## 入っているもの

[setup.md](setup.md) で入るもの。

- ESP-IDF v6.1。`cd firmware && idf.py build` が `firmware/build/compile_commands.json` を作る。C の診断はこれを入力にする。build する前は、インクルードが見つからないという誤検知になる。
- Xcode の Swift と `/usr/bin/clangd`（Xcode の Apple clangd）。
- Cursor の Swift 拡張（`swiftlang.swift-vscode`）。`mac/Package.swift` を見つけて Swift を診断する。

追加するものは clangd 拡張（`llvm-vs-code-extensions.vscode-clangd`）。`.vscode/extensions.json` が勧めるので、Cursor の案内から入れる。入れたあとはウィンドウを開き直す。

拡張が自分で配る clangd は使わない。`.vscode/settings.json` の `clangd.path` は `/usr/bin/clangd` を指す。Xtensa のターゲットで `firmware/main/main.c` の診断がソース由来ゼロになることを確認したのは、このバイナリ。

## なぜ警告がレビューに使えなかったか

言語サーバーが、このリポジトリの C をホスト Mac の普通の C として見ていた。

- Swift 拡張の SourceKit-LSP は、初期設定だと C と C++ も診断する。ワークスペースに `compile_commands.json` があると、ESP-IDF と `managed_components` の約 2000 ファイルまで対象になる。コンパイラは Xtensa 用の GCC で、ホストの Clang はそのフラグを知らない。
- clangd 拡張は入っていなかった。`compile_commands.json` は `firmware/build/` にあり、clangd がソースの親ディレクトリを探すだけでは見つからない。
- `--query-driver` が無いと、Xtensa 用 GCC のシステムヘッダとターゲットが取れない。`main.c` では、実在する `../hal.h` が見つからない、という誤検知になる。
- `firmware/main/fonts/font_ja_16.c` は生成物で約 2.7MB、1 行が 2 万文字を超える。エディタがこれを解析すると固まる。

## リポジトリに置いた設定

| ファイル | 役割 |
| --- | --- |
| `.clangd` | `firmware/build` の compile_commands を使う。Clang が知らないフラグを外す。診断は `firmware/main` の手書きだけ |
| `.vscode/settings.json` | clangd のパスと `--query-driver`。SourceKit は Swift だけ。Microsoft C/C++ 拡張を入れても IntelliSense は止める。長い行はトークナイズしない |
| `.vscode/extensions.json` | clangd 拡張と Swift 拡張を勧める |
| `.cursorignore` | ビルド成果物、`managed_components`、生成フォント、`backup/`、`references/` の clone を、エージェントの索引に入れない |

`--query-driver` の `*` は、`~/.espressif/tools/xtensa-esp-elf/` のバージョンディレクトリに合う。GCC のファイル名が `xtensa-esp32s3-elf-gcc` のままなら、ツールチェーンを入れ直しても設定は変えない。

## 問題パネルに残るもの

`firmware/main` の手書きの C と、`mac/` の Swift。生成フォント、`firmware/build`、`managed_components`、ESP-IDF のヘッダに出る警告は見ない。そこを直そうとしない。

確認したこと。

- `clangd --check=firmware/main/main.c` に、この `.clangd` と `--query-driver` を足すと、ソース由来の診断は 0 件。`--check` がコードアクションの自己テストで 1 件失敗するが、エディタの警告ではない。フラグを外す前は、未知のフラグ 7 件と、`../hal.h` が見つからない 1 件だった。
- `swift build --package-path mac` は警告なく通る。SourceKit を Swift だけにした状態で、Cursor の問題パネル自体は見ていない。

## 診断が再び崩れたとき

- `firmware/build/compile_commands.json` が無い。`cd firmware && idf.py build`。
- C の警告の出所が SourceKit になっている。`swift.sourcekit-lsp.supported-languages` が `swift` だけか確認し、ウィンドウを開き直す。
- clangd 拡張が別の clangd を使っている。`clangd.path` が `/usr/bin/clangd` か確認する。

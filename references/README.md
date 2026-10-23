# 参考リポジトリ

ビルドには使わない。出典を手元で読むために、リポジトリルートの `references/` へ clone する。clone した中身は git に入れない。

取得の流れは [docs/setup.md](../docs/setup.md) の手順 1。

## esphome-guition-jc3636k718c-va

[MichalZaniewicz/esphome-guition-jc3636k718c-va](https://github.com/MichalZaniewicz/esphome-guition-jc3636k718c-va)。Guition JC3636K718C 向けの ESPHome 構成で、メーカーデモ `JC3636K718_knob_EN` のピン配置と、実機で分かった注意点がまとまっている。ライセンスは MIT（Copyright (c) 2026 Michał Zaniewicz）。

スキルと液晶の初期化を取り込んだ時点のタグは v2.2.12（`8ffed08013312bf52d72bb2bf78dc208abf8db34`）。

```
git clone --branch v2.2.12 https://github.com/MichalZaniewicz/esphome-guition-jc3636k718c-va.git references/esphome-guition-jc3636k718c-va
```

v2.2.12 は annotated tag なので、clone 時に `is not a commit` と出る。その後の checkout 先が `8ffed08013312bf52d72bb2bf78dc208abf8db34` なら、取り込んだ版と同じ。

### 取り込んだもの

- スキル `skill/guition-jc3636k718c` を `.claude/skills/guition-jc3636k718c` にファイルとして置いた。`SKILL.md` は v2.2.12 の内容そのもの。隣の `LICENSE` は出典の MIT ライセンス。
- `base/core.yaml` のディスプレイ初期化シーケンスを `firmware/main/lcd_init_cmds.h` に起こした。
- ピン配置は `firmware/main/board.h`。値の出典は同じメーカーデモで、スキルの Pinout に書かれている。

ESPHome の YAML はビルドに使っていない。実機で上流と違ったことは [docs/hardware-notes.md](../docs/hardware-notes.md) に書く。

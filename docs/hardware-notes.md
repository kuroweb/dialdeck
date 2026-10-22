# ハードウェアの注意点

実機で確かめたこと。ピン配置は `firmware/main/board.h`、上流の知見は `.claude/skills/guition-jc3636k718c` にある。ここには、上流に無いことと、上流と違ったことを書く。

## ノブ

一般的な 2 相式のエンコーダーではない。左に回すと GPIO2、右に回すと GPIO1 が LOW になり、1 目盛りで 1 回パルスが出る。0.1 ms ごとに記録したところ、両方が同時に LOW になることはなく、パルスは短くても約 4 ms 続いた。

立ち下がりの割り込みで読むと、左に回したときに右回しとしても数えてしまい、値が増えたり減ったりした。0.1 ms の記録には出ないほど短いノイズを、割り込みが拾っていると見ている。

`firmware/main/knob.c` では、割り込みを使わず 1 ms ごとに両方のピンを読み、2 ms 続けて LOW のときだけ 1 目盛りと数える。ピンが HIGH に戻るまで次は数えない。

## GPIO 割り込み

CST816 のタッチドライバは、INT ピン（GPIO7）のために GPIO の ISR service を自分で登録する。後から `gpio_install_isr_service()` を呼ぶと `ESP_ERR_INVALID_STATE` が返るので、エラーとして止めない。

## 液晶

`esp_lcd_st77916` の既定の初期化ではなく、メーカーの初期化シーケンス（`firmware/main/lcd_init_cmds.h`）を渡す。この形で、色とタッチの座標が画面と合うことを確かめた。

初期化時に `The 3Ah command has been used and will be overwritten by external initialization sequence` という警告が出る。シーケンスの中で色形式（3Ah）を RGB565 に設定しているためで、問題ない。

## 書き込み後の起動

ダウンロードモードから書き込んだあと、USB 経由のリセットでは `boot:0x3 (DOWNLOAD(USB/UART0))` のまま止まることがある。USB ケーブルを抜き差しして電源を入れ直すと、通常どおり起動する。

## チップとメモリ

- ESP32-S3（QFN56、rev v0.2）
- PSRAM 8MB 内蔵（octal、AP 製、80MHz）
- フラッシュ 16MB（boya、QIO）

# 純正ファームウェアのバックアップ

DialDeck のファームを書き込むと、出荷時の純正ファームとパーティション構成は消える。純正に戻せるよう、初めて書き込む前にフラッシュ全体（16MB）を吸い出しておく。

esptool が必要（[セットアップ](setup.md) の手順 6）。

## 1. ダウンロードモードで起動する

純正ファームは USB を HID デバイスとして使っていて、シリアルポートが出ない。読み出すにはダウンロードモードで起動する。

1. USB-C ポート左の小さな BOOT ボタンを押したまま、USB-C ケーブルを挿す
2. 1〜2 秒したら BOOT を離す
3. シリアルポートが出たことを確認する

```
ls /dev/cu.usbmodem*
```

## 2. チップを確認する

```
esptool -p /dev/cu.usbmodem* flash-id
esptool -p /dev/cu.usbmodem* get-security-info
```

- `Detected flash size: 16MB` であること
- `Secure Boot: Disabled` と `Flash Encryption: Disabled` であること。有効だと、読めても中身は暗号化されている

## 3. フラッシュ全体を読み出す

`backup/` は `.gitignore` で除外している。nvs に Wi-Fi の認証情報などが入っている可能性があるので、コミットしない。

```
mkdir -p backup
esptool -p /dev/cu.usbmodem* -b 921600 --after no-reset read-flash 0 ALL backup/jc3636k718c_stock_<MAC>.bin
```

`<MAC>` は `flash-id` の出力の `MAC:` から `:` を除いたもの。個体ごとに分けるために付ける。

`--after no-reset` を付けると、読み出し後もダウンロードモードのままになり、続けて照合できる。

## 4. 照合する

チップ側で計算したハッシュとファイルを突き合わせる。

```
esptool -p /dev/cu.usbmodem* --after no-reset verify-flash 0 backup/jc3636k718c_stock_<MAC>.bin
```

`Verification successful (digest matched).` が出れば完了。

## 書き戻す

ダウンロードモードで起動してから書き込む。

```
esptool -p /dev/cu.usbmodem* write-flash 0 backup/jc3636k718c_stock_<MAC>.bin
```

## 取得済みのバックアップ

| 個体の MAC | ファイル | SHA-256 |
| --- | --- | --- |
| `44:bd:8d:6f:1f:b8` | `backup/jc3636k718c_stock_44bd8d6f1fb8.bin` | `73b4181484af66aa56d5650e18d4537dbb69a81ac6039b38d524f26048c3c790` |

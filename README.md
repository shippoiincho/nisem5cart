# 偽 m5 cart

![board](/pictures/board_front.jpg)

## これはなに？

SORD m5 (タカラゲームパソコン)用のマルチ ROM カートリッジです。
おまけで RGB 出力機能があります。

---
## 回路

![board](/pictures/board_back.jpg)

Z80 Bus に直結するために、
ピンの多い RP2350B のボード(WeAct RP2350B)を使用します。
カートリッジスロットの信号を、単純に Raspberry Pi Pico2 に接続しただけです。
Pico2 が 5V 耐性なのを良いことに直結しています。

```
GP0-7:  D0-7
GP8-23: A0-15
GP25: RESET
GP26: IORD
GP27: IOWR
GP28: MRD
GP29: MWR
VBUS: 5V
GND: GND
```

今回は、後半にいろいろ詰めているために Z80 Bus 用のピンを前半に詰めています。

後半はカートリッジ番号切り替えスイッチと、表示用 LED に使用しています。

```
GP40: SW1
GP41: SW2
GP42: LED1 
GP43: LED2 
GP44: LED3 
GP45: LED4 
GP46: LED5 
GP47: LED6
```

VGA 出力のためには以下のように配線します。

```
- GPIO30 VGA:H-SYNC
- GPIO31 VGA:V-SYNC
- GPIO32 VGA:Blue0 (330 Ohm)
- GPIO33 VGA:Blue1 (680 Ohm)
- GPIO34 VGA:Red0 (330 Ohm)
- GPIO35 VGA:Red1 (680 Ohm)
- GPIO36 VGA:Red2 (1.2K Ohm)
- GPIO37 VGA:Green0 (330 Ohm)
- GPIO38 VGA:Green1 (680 Ohm)
- GPIO39 VGA:Green2 (1.2K Ohm)
```

VGA の色信号は以下のように接続します

```
Blue0 --- 330 Ohm resister ---+
                              |
Blue1 --- 680 Ohm resister ---+---> VGA Blue

Red0  --- 330 Ohm resister ---+
                              |
Red1  --- 680 Ohm resister ---+
                              |
Red2  --- 1.2k Ohm resister --+---> VGA Red

Green0--- 330 Ohm resister ---+
                              |
Green1--- 680 Ohm resister ---+
                              |
Green2--- 1.2k Ohm resister --+---> VGA Green
```

このほかに VGA の GND と Pico の　GND を接続してください。

![Schematics](/pictures/m5cart_schematics.png)

---
## ROM データ

ROM データは 1 ページ最大 20KiB で、32KiB 境界ごとに置きます。
最初の ROM の場所は `0x10080000` になりますので、`0x10080000 + (0x8000 * ROM 番号)` に置いたデータが読み込まれます。
最大 64 個の ROM に対応しています。

ROM データの書き込みには [picotool](https://github.com/raspberrypi/picotool) などをご使用ください。

---
## RAM

32KiB の拡張 RAM の機能も持っています。
BASIC-F などで使用可能です。

---
## VDP エミュレーション

VDP のエミュレーションによって、m5 の出力を VGA 対応にします。
本体 VDP との同期をとっていませんので、ソフトによっては想定外の動作をすることがあります。

---
## 使用ライブラリなど

- [vrEmuTms9918](https://github.com/visrealm/vrEmuTms9918)

---
![screenshot](/pictures/screenshot.jpg)
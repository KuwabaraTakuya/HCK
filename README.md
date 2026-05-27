# HCK - Arduino × Processing 音楽合成プロジェクト

ArduinoとProcessingをシリアル通信で連携させ、楽譜データから各種楽器の音波を合成して再生するプロジェクトです。

## 概要

Arduino側で楽譜データ（MIDIノート番号・音量・音長）をシリアル出力し、Processing側でそのデータを受け取って波形合成・音声出力・波形表示を行います。

```
Arduino (楽譜データ送信) ──シリアル通信──> Processing (波形合成 + 可視化)
```

## ディレクトリ構成

```
HCK/
├── Audio_Arduino/
│   └── sheetmusic.ino      # 「カエルの歌」楽譜データをシリアル出力
├── Audio_Processing/
│   ├── wave_test.pde       # 基本合成波（サイン波の倍音合成）
│   ├── Flute_wave.pde      # フルート音色（ヴィブラート + 息ノイズ）
│   ├── Trumpet_wave.pde    # トランペット音色（鋸歯状波 + ローパスフィルタ）
│   └── Drum_wave.pde       # ドラム音色（キック / スネア）
├── LED_Arduino/            # LED制御 Arduino スケッチ（作成中）
└── LED_Processing/         # LED制御 Processing スケッチ（作成中）
```

## 音色の実装

### 基本合成波 (`wave_test.pde`)
サイン波の倍音を重ね合わせた音色。
```
samp[i] = sin(w) + sin(w*2)*0.5 + sin(w*3)*0.25 + sin(w*4)*0.1
```

### フルート (`Flute_wave.pde`)
- サイン波（90%）＋三角波（10%）の混合
- 6Hz のヴィブラート
- アタック時の息ノイズを再現

### トランペット (`Trumpet_wave.pde`)
- 鋸歯状波ベース
- 簡易ローパスフィルタでアタック時に音色が徐々に明るくなる演奏感を再現

### ドラム (`Drum_wave.pde`)
- **キック**: 150Hz → 50Hz へ急降下する低音サイン波
- **スネア**: 180Hz のサイン波 ＋ ランダムノイズ

## 使い方

### 必要なもの
- Arduino（Uno など）
- Processing 4.x
- Processing ライブラリ: [Minim](https://code.compartmental.net/minim/)

### 手順

1. **Arduino にスケッチを書き込む**
   - `Audio_Arduino/sheetmusic.ino` を Arduino IDE で開き、ボードに書き込む

2. **シリアルポートを確認・変更する**
   - 各 `.pde` ファイル内の `Serial` のポート名をご自身の環境に合わせて変更する
   ```java
   // 例: "/dev/cu.usbmodem34B7DA61F2542" → 実際のポート名に変更
   port = new Serial(this, "/dev/cu.usbmodem34B7DA61F2542", 9600);
   ```

3. **Processing でスケッチを実行する**
   - 使いたい音色の `.pde` ファイルを Processing で開いて実行
   - Drum / Flute / Trumpet は画面クリックで Arduino に開始信号を送信

## シリアルデータフォーマット

Arduino から Processing へは以下の CSV 形式で送信されます。

```
<MIDIノート番号>,<音量(0-127)>,<音長(ms)>
例: 60,100,400
```

| フィールド | 説明 |
|---|---|
| MIDIノート番号 | 60=ド, 62=レ, 64=ミ, ... / 0=休符 |
| 音量 | 0〜127 |
| 音長 | ミリ秒単位 |

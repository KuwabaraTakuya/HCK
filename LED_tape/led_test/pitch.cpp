// 音階(MIDIピッチ)を受け取り、RGB値をLEDテープに反映する関数
int convertNote(int midi_Pitch) {
  int r = 0, g = 0, b = 0;

  switch (midi_Pitch) {
    case 60: // ド (赤)
      r = 255; g = 0; b = 0; break;
    case 62: // レ (オレンジ/黄色)
      r = 255; g = 128; b = 0; break;
    case 64: // ミ (緑)
      r = 0; g = 255; b = 0; break;
    case 65: // ファ (青緑)
      r = 0; g = 255; b = 128; break;
    case 67: // ソ (青)
      r = 0; g = 0; b = 255; break;
    case 69: // ラ (紫)
      r = 128; g = 0; b = 255; break;
    case 71: // シ (ピンク)
      r = 255; g = 0; b = 128; break;
    case 72: // 高いド (赤)
      r = 255; g = 0; b = 0; break;
    default: // 休符や範囲外 (消灯)
      r = 0; g = 0; b = 0; break;
  }

  // ★ここにLEDテープへの色書き込み処理を追加します
  // 例 (Adafruit NeoPixelの場合):
  // strip.fill(strip.Color(r, g, b)); 
  // strip.show();
}
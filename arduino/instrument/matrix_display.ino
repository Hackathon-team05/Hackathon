// =============================================================================
// matrix_display.ino
//
// Arduino Uno R4 WiFi 内蔵の 12x8 ドットマトリクスLEDに文字列を表示する。
// 要: ArduinoGraphics ライブラリ（Library Manager からインストール）。
//     Arduino_LED_Matrix ライブラリは R4 WiFi のボードコアに同梱されている。
// =============================================================================
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

extern uint8_t get_instrument_id();

// instrument_id (0〜3) に対応する担当パート名を返す。
// 0=フルート 1=クラリネット 2=トランペット 3=ドラム
static const char* matrix_label_for_instrument(uint8_t instrument_id) {
  switch (instrument_id) {
    case 0: return "Fl";
    case 1: return "Cl";
    case 2: return "TP";
    case 3: return "Dr";
    default: return "??";
  }
}

// 起動時に一度だけ呼び出し、担当パート(instrument_id)に応じた文字列を
// マトリクスLEDに表示する。instrument_id_init()より後に呼び出すこと。
void matrix_display_init() {
  matrix.begin();

  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print(matrix_label_for_instrument(get_instrument_id()));
  matrix.endText();
  matrix.endDraw();
}

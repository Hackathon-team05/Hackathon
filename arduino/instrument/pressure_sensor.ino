#include<Arduino.h>

const int PRESSURE_SENSOR_PIN = A0;

//乗ってるか乗ってないかの判定
const int ONSET_THRESHOLD = 100;  //閾値（sketch_jul5a準拠）

// 重さによるピッチ変化の閾値
// 軽い : raw < LIGHT_THRESHOLD  → pitch +12（高い音）
// 普通 : LIGHT_THRESHOLD <= raw < HEAVY_THRESHOLD → pitch ±0
// 重い : raw >= HEAVY_THRESHOLD → pitch -12（低い音）
const int LIGHT_THRESHOLD = 200;   // sketch_jul5a準拠

const int HEAVY_THRESHOLD = 280;   // sketch_jul5a準拠

void pressure_init() {
  pinMode(PRESSURE_SENSOR_PIN, INPUT);
}

//従来のfrog_stateを返す関数はそのまま残す
uint8_t pressure_read() {
  int raw_value = analogRead(PRESSURE_SENSOR_PIN);
  uint8_t frog_state = 0;
  
  if(raw_value > ONSET_THRESHOLD){
    frog_state = 1;
  } else {
    frog_state = 0;
  }

  return frog_state;
}


// 【新規追加】重さに応じたピッチオフセットを返す
// 戻り値: -12（重い）/ 0（普通）/ +12（軽い）
int8_t pressure_get_pitch_offset() {
    int raw_value = analogRead(PRESSURE_SENSOR_PIN);

    if (raw_value < ONSET_THRESHOLD) {
        // 何も乗っていない → オフセットなし
        return 0;
    } else if (raw_value < LIGHT_THRESHOLD) {
        // 軽い → 高い音（+1オクターブ）
        return +12;
    } else if (raw_value < HEAVY_THRESHOLD) {
        // 普通 → そのまま
        return 0;
    } else {
        // 重い → 低い音（-1オクターブ）
        return -12;
    }
}

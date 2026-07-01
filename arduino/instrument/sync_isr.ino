// sync_isr.ino
#include "score_data.h"

const int SYNC_PIN = 2;

extern volatile bool is_playing;
extern volatile uint16_t local_tick;

void sync_init() {
    pinMode(SYNC_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SYNC_PIN), on_sync_tick, RISING);
}

// server.ino が global_tick++ のたびに CS_SYNC を HIGH パルスとして出力する。
// その立ち上がりに同期して tick を進める。
// 実際に音を鳴らす判定(score_step)は instrument.ino の loop() 側で
// local_tick の値を見て行われるため、ここでは tick の更新のみを行う。
void on_sync_tick() {
    if (is_playing) {
        local_tick++;
    }
}
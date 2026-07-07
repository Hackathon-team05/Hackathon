// score_player.ino
#include "score_data.h"
#include <avr/pgmspace.h>

// 各パートの音数は sizeof で自動算出する。
// （手動カウントだと score_data.h の増減で不一致になり配列外参照を起こすため。
//   旧実装では score_lengths[3]=32 に対し実データは24音で範囲外読みが発生していた）
const uint8_t score_lengths[4] = {
    sizeof(score_part_0) / sizeof(NoteEvent),  // メロディ
    sizeof(score_part_1) / sizeof(NoteEvent),
    sizeof(score_part_2) / sizeof(NoteEvent),
    sizeof(score_part_3) / sizeof(NoteEvent)   // ドラム（実データに自動追従）
};
static const uint8_t MAX_NOTES = 32;  // note_active[] の上限。各パート音数はこれ以下であること

const NoteEvent* my_score = NULL;
uint8_t my_score_length = 0;
bool note_active[MAX_NOTES] = {false};

// 【バグ3修正】前回のtickを記憶する静的変数（初期値は無効値 0xFFFF）
static uint16_t last_tick = 0xFFFF;

extern void serial_tx_note_on(uint8_t pitch, uint8_t velocity);
extern void serial_tx_note_off(uint8_t pitch);

void score_init(uint8_t instrument_id) {
    switch (instrument_id) {
        case 0: my_score = score_part_0; break;
        case 1: my_score = score_part_1; break;
        case 2: my_score = score_part_2; break;
        case 3: my_score = score_part_3; break;
        default: my_score = score_part_0; break;
    }
    my_score_length = score_lengths[instrument_id];

    for (uint8_t i = 0; i < my_score_length; i++) {
        note_active[i] = false;
    }
    
    // 譜面初期化時にtick記録もリセット
    last_tick = 0xFFFF;
}

void score_step(uint16_t local_tick, int8_t pitch_offset) {
    if (my_score == NULL) return;

    // 初回呼び出し時の追従処理
    if (last_tick == 0xFFFF) {
        last_tick = local_tick;
    }

    for (uint8_t i = 0; i < my_score_length; i++) {
        uint16_t start_t = pgm_read_word(&(my_score[i].start_tick));
        uint16_t end_t = pgm_read_word(&(my_score[i].end_tick));
        uint8_t pitch = pgm_read_byte(&(my_score[i].pitch));
        uint8_t velocity = pgm_read_byte(&(my_score[i].velocity));

        // オフセットを加算（0〜127の範囲にクランプ）
        int16_t shifted_pitch = (int16_t)pitch + pitch_offset;
        if (shifted_pitch < 0)   shifted_pitch = 0;
        if (shifted_pitch > 127) shifted_pitch = 127;

        // 【バグ3修正】等号一致(==)から、前回値〜今回値の範囲跨ぎ判定に変更
        // 通常の進行、および0へのラップアラウンド（ループ）を考慮した判定

        // --- NOTE_ON 判定 ---
        if (!note_active[i]) {
            if ((last_tick <= start_t && local_tick >= start_t) || 
                (last_tick > local_tick && (start_t >= last_tick || start_t <= local_tick))) {
                
                // 【バグ1修正】 pitch ではなく shifted_pitch を渡す
                serial_tx_note_on(shifted_pitch, velocity);
                note_active[i] = true;
            }
        }

        // --- NOTE_OFF 判定 ---
        if (note_active[i]) {
            if ((last_tick <= end_t && local_tick >= end_t) || 
                (last_tick > local_tick && (end_t >= last_tick || end_t <= local_tick))) {
                
                // 【バグ1修正】 pitch ではなく shifted_pitch を渡す
                serial_tx_note_off(shifted_pitch);
                note_active[i] = false;
            }
        }
    }

    // 次回ループのために現在のtickを保存
    last_tick = local_tick;
}

// 【今回の変更に伴う注記】自パート終了機能(score_data.hのPART_LOOP_COUNT)の導入により、
// instrument.ino側はもうlocal_tickを毎ループ0へ巻き戻さず、raw値のまま増加させ続けて
// 終了判定に使うようになった。そのため、この関数はinstrument.ino側からは呼ばれなくなった
// (score_step()へ渡すtickはinstrument.ino側で `local_tick % LOOP_MAX_TICK` を計算して渡す)。
// 互換性のため関数自体は残しているが、現状未使用。
void score_loop_check(volatile uint16_t &local_tick) {
    if (local_tick >= LOOP_MAX_TICK) {
        local_tick = 0;
    }
}

// アクティブな（現在鳴っている）全音符にNOTE_OFFを発行して停止する
void score_stop_all() {
    if (my_score == NULL) return;

    for (uint8_t i = 0; i < my_score_length; i++) {
        // 現在音が鳴っている（アクティブな）音符があれば
        if (note_active[i]) {
            uint8_t pitch = pgm_read_byte(&(my_score[i].pitch));

            // 安全に音を止めるためにNOTE_OFFを送信します。
            serial_tx_note_off(pitch);

            note_active[i] = false; // 未アクティブ状態に戻す
        }
    }
    // tickの記録もリセット
    last_tick = 0xFFFF;
}
// instrument_id.ino
#include <EEPROM.h>

const int EEPROM_ID_ADDR = 0x00;
const int ERROR_LED_PIN = 13;

uint8_t instrument_id = 0xFF;

uint8_t eeprom_read_instrument_id() {
    uint8_t id = EEPROM.read(EEPROM_ID_ADDR);
    if (id >= 4) {
        return 0xFF;
    }
    return id;
}

void instrument_id_init() {
    pinMode(ERROR_LED_PIN, OUTPUT);
    digitalWrite(ERROR_LED_PIN, LOW);

    EEPROM.write(EEPROM_ID_ADDR,2);//楽器番号によって数値を変更
    instrument_id = eeprom_read_instrument_id();

    if (instrument_id == 0xFF) {
        Serial.println(F("[WARN] EEPROM ID is invalid. Automatically writing default ID=0..."));
        EEPROM.write(EEPROM_ID_ADDR, 0); // EEPROMに0を書き込む
        instrument_id = 0;              // プログラム上のIDも0にする
        
        // 念のためエラーLEDを短くパカパカと点滅させて、自動書き込みが走ったことを通知
        for(int i=0; i<6; i++) {
            digitalWrite(ERROR_LED_PIN, !digitalRead(ERROR_LED_PIN));
            delay(100);
        }
        digitalWrite(ERROR_LED_PIN, LOW);
    }
}

uint8_t get_instrument_id() {
    return instrument_id;
}
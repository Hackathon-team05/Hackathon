void tick_setup(){
    last_tick_us=micros();
}

uint32_t tick_generate(){
    uint32_t count=0;
    tick_period_us = 60000000UL / (bpm * 16UL);//tick出力周期
    while (micros() - last_tick_us >= tick_period_us) {
        last_tick_us += tick_period_us;
        count++;
    }
    return count;
}
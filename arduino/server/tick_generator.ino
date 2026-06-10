void tick_setup(){
    last_tick_us=micros();
}

bool tick_generate(){
    tick_period_us = 60000000UL / (bpm * 16UL);//tick出力周期
    if (micros() - last_tick_us >= tick_period_us) {
        last_tick_us += tick_period_us;
        global_tick++;
        return true;
    }
    return false;
}
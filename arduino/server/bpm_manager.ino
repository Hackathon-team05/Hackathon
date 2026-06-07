void bpm_setup(){
    bpm = DEF_BPM;
    for (int i = 0; i < WINDOW_SIZE; i++){
        mic_window[i] = 0;
    }
    clap_count = 0;
    last_time = 0;
}

bool bpm_generate(unsigned long last_t){
    //ウィンドウを埋める作業
    if (clap_count < CLAP_MAX){
        clap_times[clap_count] = last_t;
        clap_count++;
    }else{
        for (int i = 0; i < CLAP_MAX - 1; i++){
            clap_times[i] = clap_times[i + 1];
        }
        clap_times[CLAP_MAX - 1] = last_t;
    }

    if (clap_count < CLAP_MAX){//ウィンドウの埋まり具合を検知
        return false;
    }
    float interval_sum = 0;
    for (uint8_t i = 0; i < CLAP_MAX - 1; i++){//過去ウィンドウの上書き
        interval_sum += (clap_times[i + 1] - clap_times[i]);
    }

    float avg_interval_ms = interval_sum / (CLAP_MAX - 1);
    float estimated_bpm = 60000 / avg_interval_ms;

    if (estimated_bpm < BPM_MIN){
        estimated_bpm = BPM_MIN;
    }
    if (estimated_bpm > BPM_MAX){
        estimated_bpm = BPM_MAX;
    }

    bpm =bpm * (1 - SMOOTH_FACTOR) + estimated_bpm * SMOOTH_FACTOR;
    return true;
}

float get_bpm(){
    return bpm;
}

float default_bpm(){
    bpm = DEF_BPM;
    return bpm;
}
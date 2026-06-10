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
    if(clap_count < CLAP_MAX){
        clap_times[clap_count] = last_t;
        clap_count++;
        // 5回分の拍手時刻が揃うまでBPMを計算しない
        if(clap_count < CLAP_MAX){
            return false;
        }
    }else{
        for(int i=0;i<CLAP_MAX-1;i++){
            clap_times[i] = clap_times[i+1];
        }
        clap_times[CLAP_MAX-1] = last_t;
    }

    for(uint8_t i=0;i<CLAP_INTERVAL_COUNT;i++){//拍手間隔時間の計算と格納
        clap_intervals[i] = clap_times[i+1]-clap_times[i];
        clap_sorted_intervals[i] = clap_intervals[i];
    }

    for(uint8_t i=0;i<CLAP_INTERVAL_COUNT-1;i++){//拍手間隔時間を昇順に並び替え
        for(uint8_t j=i+1;j<CLAP_INTERVAL_COUNT;j++){
            if(clap_sorted_intervals[i]>clap_sorted_intervals[j]){
                unsigned long temp = clap_sorted_intervals[i];
                clap_sorted_intervals[i] = clap_sorted_intervals[j];
                clap_sorted_intervals[j] = temp;
            }
        }
    }

    //中央値の計算（ウィンドウサイズ奇数のみ対応）
    float median_interval =((float)clap_sorted_intervals[CLAP_INTERVAL_COUNT/2-1]+(float)clap_sorted_intervals[CLAP_INTERVAL_COUNT/2])/2.0;
    //閾値の設定
    float lower_threshold =median_interval*(1.0-INTERVAL_OUTLIER_RATIO);
    float upper_threshold =median_interval*(1.0+INTERVAL_OUTLIER_RATIO);
    float interval_sum = 0;
    uint8_t valid_interval_count=0;

    for(uint8_t i=0;i<CLAP_INTERVAL_COUNT;i++){
        if(clap_intervals[i]>=lower_threshold && clap_intervals[i]<=upper_threshold){
            interval_sum += clap_intervals[i];//ウィンドウの更新
            valid_interval_count++;
        }
    }

    // 有効な間隔が少ない場合はBPMを更新しない
    if(valid_interval_count<2){
        return false;
    }

    float avg_interval_ms = interval_sum / valid_interval_count;
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
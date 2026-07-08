void mic_setup(){
    pinMode(MIC_PIN,INPUT);
    last_mic_sample_ms=millis();
}

bool mic_read(){
    unsigned long now_ms=millis();
    if(now_ms-last_mic_sample_ms<MIC_SAMPLE_INTERVAL_MS){
        return false;
    }
    last_mic_sample_ms+=MIC_SAMPLE_INTERVAL_MS;
    bool judge=false;
    unsigned long start_t=now_ms;
    value=analogRead(MIC_PIN);
    if(!win_full){
        mic_window[win_id] = value;
        win_id = (win_id + 1) % WINDOW_SIZE;
        if (win_id == 0) {
            win_full = true;
        }
        return judge;
    }
    float sum=0;
    for(int i=0;i<WINDOW_SIZE;i++){
        sum+=mic_window[i];
    }
    float mean=sum/WINDOW_SIZE;

    float var=0;
    for(int i=0;i<WINDOW_SIZE;i++){
        float d =mic_window[i]-mean;
        var+=d*d;
    }
    float sigma=sqrt(var/WINDOW_SIZE);

    float threshold=mean+PEAK_SIGMA_FACTOR*sigma;
    if(!peak_track&&value>threshold){
        peak_track=true;
        peak_max=value;
        peak_time=start_t;
        down_count=0;
        return judge;
    }else if(peak_track){
        if(value>peak_max){
            peak_max=value;
            peak_time=start_t;
            down_count=0;
        }else{
            down_count++;
        }
        if(down_count>=PEAK_DOWN_COUNT){
            if(last_time==0||peak_time-last_time>=INTERVAL_TIME){//インターバルチェック
                judge = bpm_generate(peak_time);
                last_time=peak_time;
            }
            peak_track=false;
            peak_time=0;
            peak_max=0;
            down_count=0;
        }
        return judge;//returnすることで閾値以上の値を閾値設定バッファに入れない
    }
    mic_window[win_id] = value;
    win_id = (win_id + 1) % WINDOW_SIZE;
    if (win_id == 0) {
        win_full = true;
    }
    return judge;
}
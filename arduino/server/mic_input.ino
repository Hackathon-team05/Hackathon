void mic_setup(){
    pinMode(MIC_PIN,INPUT);
}

bool mic_read(){
    bool judge=false;
    unsigned long start_t=millis();
    if(start_t-last_time<250){//インターバルチェック
        return judge;
    }
    value=analogRead(MIC_PIN);
    if(!win_full){
        mic_window[win_id] = value;
        win_id = (win_id + 1) % WINDOW_SIZE;
        if (win_id == 0) {
            win_full = true;
        }
        return 0;
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
    if(value>threshold){
        judge = bpm_generate(start_t);
        last_time=start_t;
        return judge;//returnすることで閾値以上の値を閾値設定バッファに入れない
    }
    mic_window[win_id] = value;
    win_id = (win_id + 1) % WINDOW_SIZE;
    if (win_id == 0) {
        win_full = true;
    }
    last_time=start_t;
    return judge;
}

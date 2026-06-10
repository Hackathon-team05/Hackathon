void mic_setup(){
    pinMode(MIC_PIN,INPUT);
}

bool mic_read(){
    bool judge=false;
    unsigned long start_t=millis();
    if(last_time-start_t<250){//インターバルチェックH
        return judge;
    }
    value=analogRead(MIC_PIN);
    mic_window[win_id] = value;
    win_id = (win_id + 1) % WINDOW_SIZE;
    if (win_id == 0) {
        win_full = true;
    }
    if(!win_full){
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
    }
    return judge;
}
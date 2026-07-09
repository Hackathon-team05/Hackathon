uint32_t detected_clap_total = 0;

// 【追加】Pythonスクリプト（plot_mic_results.py）連携用のログ出力
void log_mic_data_for_python(unsigned long t, uint16_t adc, bool p_track, bool updated) {
    Serial.print("MIC_DATA,");
    Serial.print(t); Serial.print(",");
    Serial.print(adc); Serial.print(",");
    Serial.print(p_track ? 1 : 0); Serial.print(",");
    Serial.print(updated ? 1 : 0); Serial.print(",");
    Serial.print(bpm); Serial.print(",");
    Serial.print(clap_count); Serial.print(",");
    Serial.println(detected_clap_total);
}

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
        log_mic_data_for_python(start_t, value, peak_track, judge);
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
        log_mic_data_for_python(start_t, value, peak_track, judge);
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
                detected_clap_total++;
                judge = bpm_generate(peak_time);
                last_time=peak_time;
            }
            peak_track=false;
            peak_time=0;
            peak_max=0;
            down_count=0;
        }
        log_mic_data_for_python(start_t, value, peak_track, judge);
        return judge;//returnすることで閾値以上の値を閾値設定バッファに入れない
    }
    mic_window[win_id] = value;
    win_id = (win_id + 1) % WINDOW_SIZE;
    if (win_id == 0) {
        win_full = true;
    }
    log_mic_data_for_python(start_t, value, peak_track, judge);
    return judge;
}

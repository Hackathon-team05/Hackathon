void LED_setup(){
    matrix.begin();
    matrix.renderBitmap(frame, 8, 12);
}

void light_up(int dev){
    const uint8_t digits[4][5] = {
        {0b010,0b110,0b010,0b010,0b111}, // 1
        {0b111,0b001,0b111,0b100,0b111}, // 2
        {0b111,0b001,0b111,0b001,0b111}, // 3
        {0b101,0b101,0b111,0b001,0b001}  // 4
    };

    for(int row=0;row<5;row++){
        for(int col=0;col<3;col++){
            frame[row+1][dev*3+col]=bitRead(digits[dev][row],2-col);
        }
    }

    matrix.renderBitmap(frame,8,12);
}

void failsafe_light(int dev){
    const uint8_t digits[4][5] = {
        {0,0,0,0,0}, // 0
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0}
    };

    for(int row=0;row<5;row++){
        for(int col=0;col<3;col++){
            frame[row+1][dev*3+col]=bitRead(digits[dev][row],2-col);
        }
    }

    matrix.renderBitmap(frame,8,12);
}
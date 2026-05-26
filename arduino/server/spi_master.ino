#include<SPI.h>

const int CS_DEV1=D4;
const int CS_DEV2=D5;
const int CS_DEV3=D6;
const int CS_DEV4=D7;
const int CS_SYNC=D9;

void spi_setup(){
    SPI.begin();

    pinMode(CS_DEV1,OUTPUT);
    pinMode(CS_DEV2,OUTPUT);
    pinMode(CS_DEV3,OUTPUT);
    pinMode(CS_DEV4,OUTPUT);
    pinMode(CS_SYNC,OUTPUT);

    digitalWrite(CS_DEV1,HIGH);
    digitalWrite(CS_DEV2,HIGH);
    digitalWrite(CS_DEV3,HIGH);
    digitalWrite(CS_DEV4,HIGH);
    digitalWrite(CS_SYNC,HIGH);
};



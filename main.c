/* 
 * File:   main.c
 * Author: Chan
 *
 * Created on November 16, 2017, 10:30 AM
 */

/*___________________________SPEAKER_TEMP_____________________________*/


#include <p18f452.h>
#include "xlcd.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <delays.h>
#include <float.h>
#include "ow.h"
#include <pwm.h>
#include <timers.h>

#pragma config OSC = HS
#pragma config WDT = OFF
#pragma config LVP = OFF

#define _XTAL_FREQ 4000000

//The note for a medical alarm is C4
#define C4 0xED         //This value was determined using a prescalar of 16 and 4MHz clock.

//Global Vars
char song[3]={C4, C4, C4}; //alarm pattern
int i;
char k[1];
unsigned char TemperatureLSB;
unsigned char TemperatureMSB;
unsigned int tempint0 =0;
unsigned int tempint1 = 0;
unsigned int tempint = 0;
float fractionFloat = 0.0000;
int fraction = 0;
int x = 0;
unsigned char aveLSB = 0;
unsigned char aveMSB = 0;
unsigned short int sum = 0;
unsigned short int temp = 0;
unsigned short int average = 0;
unsigned char degree = 0xDF;
char tempDisplay[20];
char ClrScn[20] = "                   ";

/*---------------------------Speaker---------------------------*/

void spkr_setup(void){
    TRISCbits.TRISC2 = 0;
    SetDCPWM1(50);
    OpenTimer2(TIMER_INT_OFF & T2_PS_1_16 & T2_POST_1_1);
}

void sound_spkr(void){
    for (i=0; i<3; i++) {       //play x notes inside song array
      
        OpenPWM1(song[i]); 
                                //set PWM frequency according to entries in song array
        Delay10KTCYx(40);       //note is played for 400ms

        OpenPWM1(1);            //the PWM frequency set beyond audible range
                                //in order to create a short silence between notes
        Delay10KTCYx(40);       //play nothing for 400 ms                  
        }
    Delay10KTCYx(40);
}


/*---------------------------Delays---------------------------*/
void DelayFor18TCY(void)
{
    Delay1TCY();
 	Delay1TCY();
    Delay1TCY();
    Delay1TCY();
 	Delay1TCY();
    Delay1TCY();
 	Delay1TCY();
 	Delay1TCY();
 	Delay10TCYx(1);
}
 
void DelayXLCD(void)     // minimum 5ms
{
    Delay1KTCYx(5); 		// Delay of 5ms
                            // Cycles = (TimeDelay * Fosc) / 4
                            // Cycles = (5ms * 4MHz) / 4
                            // Cycles = 5,000

}

void DelayPORXLCD(void)   // minimum 15ms
{
    Delay1KTCYx(15);		// Delay of 15ms
                            // Cycles = (TimeDelay * Fosc) / 4
                            // Cycles = (15ms * 4MHz) / 4
                            // Cycles = 15,000

}

/*---------------------------LCD---------------------------*/

void LCD_setup(void){
    PORTD = 0X00;
    TRISD = 0x00;
    
    OpenXLCD(FOUR_BIT & LINES_5X7);
    while(BusyXLCD());
    SetDDRamAddr(0x00);              //Start writing at top left hand side of screen
    WriteCmdXLCD( SHIFT_DISP_LEFT );
    while(BusyXLCD());
    WriteCmdXLCD( BLINK_ON );
    while(BusyXLCD());

 }

void clrscn(void){
    while(BusyXLCD());
    SetDDRamAddr(0x00);        //1st row
    while(BusyXLCD());
    putsXLCD(ClrScn);           //clear row

    while(BusyXLCD());
    SetDDRamAddr(0x40);        //2nd row
    while(BusyXLCD());
    putsXLCD(ClrScn);           //clear row

    while(BusyXLCD());
    SetDDRamAddr(0x10);        //3rd row
    while(BusyXLCD());
    putsXLCD(ClrScn);           //clear row

    while(BusyXLCD());
    SetDDRamAddr(0x50);        //4th row
    while(BusyXLCD());
    putsXLCD(ClrScn);          //clear row
}
        
 
 /*------------------------------Temp------------------------------*/

void cnvtTemp(void){
    // convert Temperature
    ow_reset();                 //send reset signal to temp sensor
    ow_write_byte(0xCC);        // address all devices w/o checking ROM id.
    ow_write_byte(0x44);        //start temperature conversion 
    PORTBbits.RB3 = 1;          //power sensor from parasitic power during conversion
    Delay10KTCYx(75);           //wait the recommended 750ms for temp conversion to complete
    PORTBbits.RB3 = 0;          // turn off parasitic power

    // read Temperature
    ow_reset();                 //send reset signal to temp sensor
    ow_write_byte(0xCC);        // address all devices w/o checking ROM id.
    ow_write_byte(0xBE);        //read scratchpad 
    TemperatureLSB = ow_read_byte();   //read byte 0 which is temperature LSB
    TemperatureMSB = ow_read_byte();   //read byte 1 which is temperature MSB
    //TemperatureLSB = 0x30;        //test values
    //TemperatureMSB = 0x02;
}

void cnvtTemp16(void){
    for (x = 1; x <= 16; x++)
        {
            cnvtTemp();
            
            temp = TemperatureMSB;
            temp = temp << 8;
            temp = temp | TemperatureLSB;
            
            sum += temp;
        }
        
        average = sum/16;
        
        
        aveLSB = average;
        aveMSB = average >> 8;
}


void interpretTemp(void){
    //Interpret temperature reading
    tempint0 = aveLSB >> 4;          //shift bits to get the integer part of the reading
    tempint1 = aveMSB << 4;
    tempint = tempint1 |tempint0;            //combine the integer vars to get true integer reading
    
    if(aveLSB & 0x01)               //mask and test bits to get fractional part of reading
        fractionFloat += 0.0625;

    if(aveLSB & 0x02)
        fractionFloat += 0.125;

    if(aveLSB & 0x04)
        fractionFloat += 0.25;

    if(aveLSB & 0x08)
        fractionFloat += 0.5;

    fraction =fractionFloat*1000;         //convert the fraction value to an int for display
}

void disp_Temp(void){
        
    while(BusyXLCD());
    SetDDRamAddr(0x00);         //Start writing at top left hand side of screen
    while(BusyXLCD());
    Delay1KTCYx(50);            //Delay prevent screen from flickering.
    while(BusyXLCD());
    putsXLCD(tempDisplay);      // Show current temperature
    while(BusyXLCD());
    sprintf(tempDisplay,"                   ");     //clear screen
        
}


/*---------------------------main-----------------------*/
void main (void)
{ 
    //system config
     //Enable interrupts
    INTCONbits.GIE = 1;
    INTCONbits.INT0IF = 0;
    INTCONbits.INT0IE = 1;
    INTCON2bits.INTEDG0 = 0;
    
    TRISCbits.RC7=1;    //Set PORT C pins to inputs
    TRISCbits.RC6=1;
    TRISCbits.RC5=1;
    TRISCbits.RC4=1;
    TRISBbits.RB3=0;    //Used to power temp sensor for a reading
    
    //setup LCD and spkr
    LCD_setup();
    spkr_setup();
    
    
    while(1){
        
        cnvtTemp16();               //convert 16 temperature readings
        
        interpretTemp();            //convert the average temperature into a displayable format.
        
        //string to display on screen
        sprintf(tempDisplay,"Temp: +%d.%03d%cC",tempint,fraction,degree);
        disp_Temp();
        
        
        clrscn();       

        
        if (tempint>26)
            sound_spkr();       
        
    }
}


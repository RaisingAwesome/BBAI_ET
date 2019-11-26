////////////////////////////////////////
//  servoPot.c
//      reads a pot and translates it to
//      a servos position.
//  Author:  Sean J. Miller, 11/3/2019
//  Wiring: Jumper P9.14 to a servo signal through a 220ohm resistor
//          Hook a potentiometers variable voltage to P9.33 (analog in)
//  See: https://www.element14.com/community/community/designcenter/single-board-computers/next-genbeaglebone/blog/2019/10/27/beagleboard-ai-brick-recovery-procedure
////////////////////////////////////////
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
int analogRead(){
    int i;
    FILE * my_in_pin = fopen("/sys/bus/iio/devices/iio:device0/in_voltage7_raw", "r");//P9.33 on the BBAI
    char the_voltage[5];//the characters in the file are 0 to 4095
    fgets(the_voltage,6,my_in_pin);
    fclose(my_in_pin);
    //printf("Voltage:  %s\n", the_voltage);
    sscanf(the_voltage, "%d", &i);
    return (i);
}
void pwm_duty(int the_duty_multiplier)
{
    FILE *duty; int duty_calc;
    duty = fopen("/sys/class/pwm/pwm-0:0/duty_cycle", "w");
    fseek(duty,0,SEEK_SET);
    duty_calc=(600000 + (1700000*(float)((float)the_duty_multiplier/4095))) ;
    //printf("Duty: %d\n", duty_calc);//1ms
    fprintf(duty,"%d",duty_calc);//1ms
    fflush(duty);
    fclose(duty);
}

void setupPWM() {
    FILE *period, *pwm;
    pwm_duty(2000);
    
    period = fopen("/sys/class/pwm/pwm-0:0/period", "w");
    usleep(20000);
    fseek(period,0,SEEK_SET);
    usleep(20000);
    fprintf(period,"%d",20000000);//20ms
    usleep(20000);
    fflush(period);
    fclose(period);

    pwm = fopen("/sys/class/pwm/pwm-0:0/enable", "w");
    usleep(20000);
    fseek(pwm,0,SEEK_SET);
    usleep(20000);
    fprintf(pwm,"%d",1);
    usleep(20000);
    fflush(pwm);
    
    fclose(pwm);
}

void recordRotation(int the_rotation){
	char buffer[64];
	//printf ("In rotation: %d\n",the_rotation);
	the_rotation=(int)((((float)the_rotation)/(float)4440)*180);
//	printf ("Rotation: %d\n",the_rotation);
	int ret = snprintf(buffer, sizeof buffer, "%d", the_rotation);
	int fp = open("/home/debian/ramdisk/bbaibackupcam_rotation", O_WRONLY|O_CREAT,0777 );
    if (fp>-1){
	    write(fp, buffer, sizeof(buffer));
	    close(fp);
    }
}

int main() {
     int ii=0;
     //printf("Setting up\n");
     setupPWM();
     
     while(1) {
        ii=analogRead();
        if (ii>1310) ii=1310;
        if (ii<140) ii=140;
        //printf("ii:%d\n",ii);
        pwm_duty(ii*2);
        recordRotation(ii*2);
        usleep(20000);
     }
}
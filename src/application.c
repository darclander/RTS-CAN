#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    Object super;
    int count;
    char c;
} App;

typedef struct {
	Object super;
	int volume;
	int kill;
	int deadline; //should be removed if not used
	int period;
} Sound;

typedef struct {
	Object super;
	int background_loop_range;
	int deadline;
} BGOverload;

typedef struct {
	Object super;
	int key_val;
	int tune_val;
	int bpm_val;
} Music;

#define MIN_IND -10
#define MAX_IND 14
#define DAC_reg ((volatile unsigned char*) 0x4000741C)

int tune[32] = {0,2,4,0,0,2,4,0,4,5,
				7,4,5,7,7,9,7,5,4,0,7,9,
				7,5,4,0,0,-5,0,0,-5,0};
				
int beat[32] = {500,500,500,500,500,500,500,500,500,500,1000,500,500,1000,250,250,250,250,500,500,250,250,250,250,
				500,500,500,500,1000,500,500,1000};
				
int period[25] = {2024,
1911,
1804,
1703,
1607,
1517,
1432,
1351,
1276,
1204,
1136,
1073,
1012,
956,
902,
851,
804,
758,
716,
676,
638,
602,
568,
536,
506};

App app = { initObject(), 0, 'X'};

void reader(App*, int);
void receiver(App*, int);

Sound sound = { initObject(), 0x8, 0, 100, 1136};
Music music = { initObject(), 0, 0, 120};
void VOLUMEchange(Sound*, int);
void VOLUMEmute(Sound*, int);

BGOverload bgoverload = { initObject(), 1000, 1300};

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);

char nullbuf(char *buf, int x) {
	for (int o = 0; o<x; o++){
		buf[o] = NULL;
	}
	return buf;
}

void writef(int msg, int x, char type) {
	SCI_WRITE(&sci0, "\n");
	char buf[x];
	if (type=='c') {
		snprintf(buf, x, "%c", msg);
	}
	else if (type=='i') {
		snprintf(buf, x, "%d", msg);
	}
	else {
		SCI_WRITE(&sci0, "s");
	}
	for(int j = 0; buf[j] != NULL; j++){
		SCI_WRITECHAR(&sci0, buf[j]);
	}
	SCI_WRITE(&sci0, "\n");
}

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

int myNum;
char buf[30];
int i = 0;
int temp = 0;

void DACfunc(Sound *self, int n) {
	if (n && !(self->kill)) {
		*DAC_reg = self->volume;
	}
	else {
		*DAC_reg = 0;
	} 
	
	SEND(USEC(self->period), 0, &sound, DACfunc, ~n);
}

void VOLUMEchange(Sound *self, int n) {
	if (self->volume >= 0x14) {
		SCI_WRITE(&sci0, "TOO LOUD ERROR\n");
		self->volume = 0x13;
		return;
	}
	if (self->volume <= 0){
		SCI_WRITE(&sci0, "TOO QUIET ERROR\n");
		self->volume = 0x1;
		return;
	}
	
	self->volume = self->volume + n;
	return;
}

void VOLUMEmute(Sound *self, int n) {
	self->kill = ~self->kill;
	return;
}

void VOLUMEon(Sound *self, int n) {
	self->kill = 0;
	return;
}

void VOLUMEoff(Sound *self, int n) {
	self->kill = 1;
	return;
}

void KEYchange(Music *self, int n) {
	if(n > 5) {
		SCI_WRITE(&sci0, "Key too high, is now set to 5\n");
		self->key_val = 5;
	}
	else if(n < -5){
		SCI_WRITE(&sci0, "Key too low, is now set to -5\n");
		self->key_val = -5;
	}
	else{
	self->key_val = n;
	}
	return;
}

void background_overload(BGOverload *self, int n) {
	
	while(n>0) {
		n--;
		
	}
	n = self->background_loop_range;

	SEND(USEC(1300),USEC(self->deadline), self, background_overload, n);
//	SYNC(&bgoverload, background_overload, n);
	return;
}

void BGchange(BGOverload *self, int  n){
	char buf[40];
	self->background_loop_range += n;
	if(self->background_loop_range > 8000){
		SCI_WRITE(&sci0, "TOO MUCH BG ERROR\n");
		self->background_loop_range = 8000;
	}
	if(self->background_loop_range < 1000){
		SCI_WRITE(&sci0, "TOO LITTLE BG ERROR\n");
		self->background_loop_range = 1000;
	}
		snprintf(buf, 40, "%d",self->background_loop_range);
		for (int j = 0; j<4; j++) {
			SCI_WRITECHAR(&sci0, buf[j]);
		}
		
	return;
}

void stune(Sound *self, int n) {
	self->period = n;
}

void BGDEADLINEchange(BGOverload *self, int n){
	if(self->deadline)
		self->deadline = 0;
	else 
		self->deadline = 1300;
	
}
void SOUNDDEADLINEchange(Sound *self, int n){
	if(self->deadline)
		self->deadline = 0;
	else 
		self->deadline = 100;
}

void BPMchange(Music *self, int n) {
	if(n > 240) {
		SCI_WRITE(&sci0, "BPM too high, is now set to 240\n");
		self->bpm_val = 240;
	}
	else if(n < 60){
		SCI_WRITE(&sci0, "BPM too low, is now set to 60\n");
		self->bpm_val = 60;
	}
	else{
	self->bpm_val = n;
	}
	return;
}

void player(Music *self, int n) {
	int p_tune = tune[n];
	int p_period = period[p_tune + self->key_val + 10];
	
	float p_beat = beat[n];
	p_beat = ((60000/self->bpm_val)*(p_beat/500));

	ASYNC(&sound, stune, p_period);
	ASYNC(&sound, VOLUMEon, 0);
	
	SEND(MSEC(p_beat-50),0, &sound, VOLUMEoff, 0);
	n = n + 1;
	
	if (n>=32) {
		n = 0;
	}
	
	SEND(MSEC(p_beat),0, &music, player, n);
}

int volume = 0x5;
void reader(App *self, int c) {
	buf[i] = c;
	int key = 0;
	key = atoi(buf);

	SCI_WRITE(&sci0, "\n");
	SCI_WRITE(&sci0, "\n");
	SCI_WRITE(&sci0, "\n");

	if (buf[i] == 'f') {
		ASYNC(&sound, DACfunc, 0);
		ASYNC(&music, player, 0);
		if(key <= 5 && key >= -5){ 
				int ind = 0;
				
				for (int u=0; u<32; u++) {
					ind = (tune[u]+key);
					temp = period[ind+10];
					writef(temp, 30, 'i');

				}
		}
		nullbuf(buf, 30);
		i = 0;
		return;
	}
	
	if(buf[i] == 't'){
		int number = atoi(buf);
		writef(number,4,'i');
		nullbuf(buf, 30);
		ASYNC(&music, BPMchange, number);
		i = 0;
		return;
	}
	
	if(buf[i] == 'y'){
		int number = atoi(buf);
		writef(number,4,'i');
		nullbuf(buf, 30);
		ASYNC(&music, KEYchange, number);
		i = 0;
		return;
	}
	
	if(buf[i] == 'o') {
		ASYNC(&sound, VOLUMEchange, -0x1);
	}
	if(buf[i] == 'p') {
		ASYNC(&sound, VOLUMEchange, 0x1);
	}
	if(buf[i] == 'm'){
		ASYNC(&sound, VOLUMEmute, 0);
	}
	if(buf[i] == 'k'){
		ASYNC(&bgoverload, BGchange, -500);
	}
	if(buf[i] == 'l'){
		ASYNC(&bgoverload, BGchange, 500);
	}
	if(buf[i] == 'v'){
		ASYNC(&bgoverload, BGDEADLINEchange, 0);
	}
	if(buf[i] == 'b'){
		ASYNC(&sound, SOUNDDEADLINEchange, 0);
	}

	i = i + 1;
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
}

void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");

    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}

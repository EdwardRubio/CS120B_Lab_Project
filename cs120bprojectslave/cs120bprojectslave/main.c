#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "includes/io.h"
#include "includes/io.c"

#define ACKLeft 0x01
#define ACKRight 0x02


//===================================GLOBAL VARIABLES=======================================//
volatile unsigned char TimerFlag = 0;
// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

unsigned char leftFlag = 0;		//Out
unsigned char rightFlag = 0;	//Out
unsigned char resetFlag = 0;	//Out
unsigned char jumpFlag = 0;		//Out
unsigned char crouchFlag = 0;	//Out
unsigned char motorFlag = 0;	//In
unsigned char scoreFlag = 0;	//In
unsigned char winFlag = 0;		//Out
unsigned char deathFlag = 0;	//In

unsigned char scoreReset = 0;

//==========================================================================================//

//========================================TASKS=============================================//
typedef struct task {
	int state;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
} task;

task tasks[7];

const unsigned char tasksNum = 7;
const unsigned long tasksPeriodGCD = 1;
const unsigned long periodJoystick = 10;
const unsigned long periodJump = 30;
const unsigned long periodCrouch = 30;
const unsigned long periodReset = 30;
const unsigned long periodTransmission = 1;
const unsigned long periodMotor = 30;
const unsigned long periodScore = 30;

//===========================================================================================//

//==========================INITIALIZATION AND UTILITY FUNCTIONS============================//
unsigned long int findGCD (unsigned long int a,
unsigned long int b)
{
	unsigned long int c;
	while(1){
		c = a%b;
		if(c==0){return b;}
		a = b;
		b = c;
	}
	return 0;
}

//ADC
void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: setting this bit enables analog-to-digital conversion.
	// ADSC: setting this bit starts the first conversion.
	// ADATE: setting this bit enables auto-triggering. Since we are
	// in Free Running Mode, a new conversion will trigger
	// whenever the previous conversion completes.
}

//TIMER
void TimerOn() {

	TCCR1B = 0x0B;

	OCR1A = 125;

	TIMSK1 = 0x02;

	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;

	SREG |= 0x80;
}
void TimerOff() {

	TCCR1B = 0x00;
}
void TimerISR() {
	unsigned char i;
	for (i = 0; i < tasksNum; ++i) {
		if( tasks[i].elapsedTime >= tasks[i].period){
			tasks[i].state = tasks[i].TickFct(tasks[i].state);
			tasks[i].elapsedTime = 0;
		}
		tasks[i].elapsedTime += tasksPeriodGCD;
	}
}
// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {

	_avr_timer_cntcurr--;
	if (_avr_timer_cntcurr == 0) {
		// Call the ISR that the user uses
		TimerISR();
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

//SPI
void SPI_SlaveInit(void) {
	DDRB = (1<<DDB6);
	SPCR = (1<<SPE);
}

unsigned char SPI_Transmit(unsigned char data){
	SPDR = data;
	while( !(SPSR & (1<<SPIF) ));
	return SPDR;
}

void transmit_data(unsigned char data){
	int i;
	for(i = 7; i >= 0; --i){
		PORTD = 0x08;
		PORTD |= ((data >> i) & 0x01);
		PORTD |= 0x04;
	}
	PORTD |=0x02;
	PORTD = 0x00;
}

void writeScore(unsigned char givenNum){
	unsigned char data = 0x00;
	switch(givenNum){
		case 0:
		data = 0x7E;
		break;
		case 1:
		data = 0x48;
		break;
		case 2:
		data = 0x3D;
		break;
		case 3:
		data = 0x6D;
		break;
		case 4:
		data = 0x4B;
		break;
		case 5:
		data = 0x67;
		break;
		case 6:
		data = 0x77;
		break;
		case 7:
		data = 0x4C;
		break;
		case 8:
		data = 0x7F;
		break;
		case 9:
		data = 0x4F;
		break;
		default:
		data = 0x00;
		break;
	}

	transmit_data((~data));

}

//==========================================================================================//

//===================================STATE MACHINE FUNCTION PROTOTYPES=======================//

enum JS_States {JS_SMStart, JS_Wait};
int TickFct_Joystick(int state);

enum JP_States {JP_SMStart, JP_Wait};
int TickFct_Jump(int state);

enum CH_States {CH_SMStart, CH_Wait};
int TickFct_Crouch(int state);

enum RT_States {RT_SMStart, RT_Wait};
int TickFct_Reset(int state);

enum TN_States {TN_SMStart, TN_Wait};
int TickFct_Transmission(int state);

enum MR_States {MR_SMStart, MR_Wait};
int TickFct_Motor(int state);

enum SE_States {SE_SMStart, SE_Wait};
int TickFct_Score(int state);

enum TR_States {TR_SMStart, TR_Wait};
int TickFct_Timer(int state);

//===========================================================================================//

//========================================MAIN===============================================//

int main(void)
{
	
	DDRA = 0x00; PORTA = 0xFF;	//Input Port, joystick and buttons.
	DDRB = 0xFF; PORTB = 0x00;	//Output Port, test output to led.
	DDRD = 0xFF; PORTD = 0x00;
	DDRC = 0xFF; PORTC = 0x00;	//C0 is motor.

	unsigned char i=0;
	tasks[i].state = JS_SMStart;
	tasks[i].period = periodJoystick;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Joystick;
	i++;
	tasks[i].state = JP_SMStart;
	tasks[i].period = periodJump;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Jump;
	i++;
	tasks[i].state = CH_SMStart;
	tasks[i].period = periodCrouch;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Crouch;
	i++;
	tasks[i].state = RT_SMStart;
	tasks[i].period = periodReset;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Reset;
	i++;
	tasks[i].state = TN_SMStart;
	tasks[i].period = periodTransmission;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Transmission;
	i++;
	tasks[i].state = MR_SMStart;
	tasks[i].period = periodMotor;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Motor;
	i++;
	tasks[i].state = SE_SMStart;
	tasks[i].period = periodScore;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Score;

	TimerSet(tasksPeriodGCD);
	TimerOn();
	ADC_init();
	SPI_SlaveInit();

	while (1)
	{
		while(!TimerFlag);
		TimerFlag = 0;
	}

	return 0;
}

//===========================================================================================//

//============================STATE MACHINE FUNCTION DEFINITIONS=============================//

int TickFct_Joystick(int state){

	switch(state){	//Transitions
		case JS_SMStart:
		state = JS_Wait;
		break;
		
		case JS_Wait:
		state = JS_Wait;
		if(ADC > 600){
			rightFlag = 1;
		}
		if(ADC < 400){
			leftFlag = 1;
		}
		
		break;
		
		default:
		state = JS_SMStart;
		break;
	};

	return state;
}

int TickFct_Crouch(int state){
	switch(state){	//Transitions
		case CH_SMStart:
		state = CH_Wait;
		break;

		case CH_Wait:
		if(((~PINA)&0x02)==0x02){
			crouchFlag = 1;
		}
		break;

		default:
		state = CH_SMStart;
		break;
	}

	return state;
}

int TickFct_Jump(int state){
	switch(state){	//Transitions
		case JP_SMStart:
		state = JP_Wait;
		break;

		case JP_Wait:
		if(((~PINA)&0x04)==0x04){
			jumpFlag = 1;
		}
		break;

		default:
		state = JP_SMStart;
		break;
	}

	return state;
}

int TickFct_Reset(int state){
	switch(state){	//Transitions
		case RT_SMStart:
		state = RT_Wait;
		break;

		case RT_Wait:
		if(((~PINA)&0x08)==0x08){
			resetFlag = 1;
		}
		break;

		default:
		state = RT_SMStart;
		break;
	}

	return state;
}

int TickFct_Transmission(int state){
	
	unsigned char dataIn = 0;
	unsigned char dataOut = 0;

	switch(state){	//Transitions
		case TN_SMStart:
		state = TN_Wait;
		break;

		case TN_Wait:
		//first bit is right.
		//second bit is left.
		//third bit is crouch.
		//fourth bit is jump.
		//fifth bit is reset.
		//sixth bit is win.

		if(rightFlag){
			rightFlag = 0;
			dataOut = dataOut | 0x01;
		}
		if(leftFlag){
			leftFlag = 0;
			dataOut = dataOut | 0x02;
		}
		if(crouchFlag){
			crouchFlag = 0;
			dataOut = dataOut | 0x04;
		}
		if(jumpFlag){
			jumpFlag = 0;
			dataOut = dataOut | 0x08;
		}
		if(resetFlag){
			resetFlag = 0;
			scoreReset = 1;
			dataOut = dataOut | 0x10;
		}
		if(winFlag){
			winFlag = 0;
			dataOut = dataOut | 0x20;
		}

		dataIn = SPI_Transmit(dataOut);

		if((dataIn&0x01)==0x01){
			motorFlag = 1;
		}

		if((dataIn&0x02)==0x02){
			scoreFlag = 1;
			motorFlag = 1;
		}

		if((dataIn&0x04)==0x04){
			scoreReset = 1;
		}

		if((dataIn&0x08)==0x08){
			scoreReset = 1;
		}
		break;

		default:
		state = TN_SMStart;
		break;
	}

	return state;
}

int TickFct_Motor(int state){

	static unsigned char motor_cnt = 0;
	
	switch(state){
		case MR_SMStart:
		state = MR_Wait;
		break;

		case MR_Wait:

		state = MR_Wait;
		if(motorFlag){
			motorFlag = 0;
			motor_cnt = 30;
		}

		if(motor_cnt != 0){
			motor_cnt--;
			PORTC = PORTC & 0xFE;
			}else{
			PORTC = PORTC | 0x01;
		}
		
		break;

		default:
		state = MR_SMStart;
		break;
	}

	return state;
}

int TickFct_Score(int state){

	static unsigned char scoreTotal = 0;
	
	switch(state){
		case SE_SMStart:
		state = SE_Wait;
		break;
		
		case SE_Wait:
		state = SE_Wait;

		writeScore(scoreTotal);

		if(scoreFlag&&scoreTotal < 5){
			scoreFlag = 0;
			scoreTotal++;
		}

		if(scoreTotal == 5){
			winFlag = 1;
			resetFlag = 1;
		}

		if(scoreReset){
			scoreTotal = 0;
			scoreReset = 0;
		}

		break;

		default:
		state = SE_SMStart;
		break;
	}

	return state;
}

//===========================================================================================//
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include "includes/io.h"
#include "includes/io.c"

#define ACKLeft 0x01
#define ACKRight 0x02

//===================================GLOBAL VARIABLES=======================================//
volatile unsigned char TimerFlag = 0;
// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

unsigned char leftFlag = 0;		//In
unsigned char rightFlag = 0;	//In
unsigned char resetFlag = 0;	//In
unsigned char jumpFlag = 0;		//In
unsigned char crouchFlag = 0;	//In
unsigned char motorFlag = 0;	//Out
unsigned char deathFlag = 0;	//Out
unsigned char winFlag = 0;		//In

unsigned char scoreFlag = 0;	//Out
unsigned char greenOutFlag = 0;
unsigned char redOutFlag = 0;

typedef struct collisionObject {
	unsigned char pastCol;
	unsigned char pastRow;
	unsigned char currCol;
	unsigned char currRow;
	unsigned char color;
	unsigned char id;					//0 is player, 1 is terrain, 2 is enemy, and 3 is point.
	unsigned char visible;
	//Save spawn location in struct and in reset reset the currCol and currRow to spawn and reset other stuff depending on id.
	unsigned char spawnCol;
	unsigned char spawnRow;
	unsigned char jump_cnt;
	unsigned char landed;
	unsigned char direction;
	unsigned char left;
	unsigned char right;
	unsigned char changeDir;
	unsigned char spawned;
	unsigned char collide;
} collisionObject;

collisionObject collisionObjects[100];

unsigned char collisionObjectsUsed = 0;

unsigned char playerCol = 0x08;
unsigned char playerRow = 0x80;

unsigned long gravityCounter = 0;
unsigned long movementCounter = 0;

//==========================================================================================//

//========================================TASKS=============================================//
typedef struct task {
	int state;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
} task;

task tasks[3];

const unsigned char tasksNum = 3;
const unsigned long tasksPeriodGCD = 1;
// const unsigned long periodJoystick = 10;
// const unsigned long periodJump = 30;
// const unsigned long periodCrouch = 30;
// const unsigned long periodReset = 30;
const unsigned long periodTransmission = 1;
const unsigned long periodGameLogic = 30;
const unsigned long periodMatrix = 1;

//===========================================================================================//

//==========================INITIALIZATION AND UTILITY FUNCTIONS============================//

void drawRed(unsigned char givenRow, unsigned char givenCol){
	PORTA = ~(givenRow);
	PORTC = givenCol;
	PORTD = ~(0x00);
}

void drawGreen(unsigned char givenRow, unsigned char givenCol){
	PORTD = ~(givenRow);
	PORTC = givenCol;
	PORTA = ~(0x00);
}

void drawYellow(unsigned char givenRow, unsigned char givenCol){
	PORTC = givenCol;
	PORTA = ~(givenRow);
	PORTD = ~(givenRow);

}

void resetCollisionObjects(){
	for(unsigned char cnt_collision = 0; cnt_collision < collisionObjectsUsed; cnt_collision++){
		if(collisionObjects[cnt_collision].id == 0||collisionObjects[cnt_collision].id == 2||collisionObjects[cnt_collision].id == 3){
			collisionObjects[cnt_collision].currCol = collisionObjects[cnt_collision].spawnCol;
			collisionObjects[cnt_collision].currRow = collisionObjects[cnt_collision].spawnRow;
			collisionObjects[cnt_collision].pastCol = collisionObjects[cnt_collision].spawnCol;
			collisionObjects[cnt_collision].pastRow = collisionObjects[cnt_collision].spawnRow;
			collisionObjects[cnt_collision].visible = 1;
		}
	}
}

void playerInput(){

	if(crouchFlag){
		crouchFlag = 0;
		motorFlag = 1;

	}

	if(resetFlag){
		resetFlag = 0;
		resetCollisionObjects();
	}

	if(leftFlag){
		leftFlag = 0;
		collisionObjects[0].left = 1;
	}

	if(rightFlag){
		rightFlag = 0;
		collisionObjects[0].right = 1;
	}

	if(jumpFlag){
		jumpFlag = 0;
		if(collisionObjects[0].jump_cnt==0x00&&collisionObjects[0].landed){
			collisionObjects[0].jump_cnt = 5;
			collisionObjects[0].landed = 0;
		}
	}
}

void movement(){
	movementCounter++;

	for(unsigned char collision_cnt = 0; collision_cnt < collisionObjectsUsed; collision_cnt++){
		if(collisionObjects[collision_cnt].id == 0x00 || collisionObjects[collision_cnt].id == 0x02){
			if(collisionObjects[collision_cnt].left){
				collisionObjects[collision_cnt].left = 0;
				if(collisionObjects[collision_cnt].currCol < 0x80){
					if(collisionObjects[collision_cnt].landed){
						if(movementCounter%3==0){
							collisionObjects[collision_cnt].pastCol = collisionObjects[collision_cnt].currCol;
							collisionObjects[collision_cnt].pastRow = collisionObjects[collision_cnt].currRow;
							collisionObjects[collision_cnt].currCol = (collisionObjects[collision_cnt].currCol << 1);
						}
						}else{
						if(movementCounter%6==0){
							collisionObjects[collision_cnt].pastCol = collisionObjects[collision_cnt].currCol;
							collisionObjects[collision_cnt].pastRow = collisionObjects[collision_cnt].currRow;
							collisionObjects[collision_cnt].currCol = (collisionObjects[collision_cnt].currCol << 1);
						}
					}
					}else{
					collisionObjects[collision_cnt].collide = 1;
				}
			}

			if(collisionObjects[collision_cnt].right){
				collisionObjects[collision_cnt].right = 0;
				if(collisionObjects[collision_cnt].currCol > 0x01){
					if(collisionObjects[collision_cnt].landed){
						if(movementCounter%3==0){
							collisionObjects[collision_cnt].pastCol = collisionObjects[collision_cnt].currCol;
							collisionObjects[collision_cnt].pastRow = collisionObjects[collision_cnt].currRow;
							collisionObjects[collision_cnt].currCol = (collisionObjects[collision_cnt].currCol >> 1);
						}
						}else{
						if(movementCounter%6==0){
							collisionObjects[collision_cnt].pastCol = collisionObjects[collision_cnt].currCol;
							collisionObjects[collision_cnt].pastRow = collisionObjects[collision_cnt].currRow;
							collisionObjects[collision_cnt].currCol = (collisionObjects[collision_cnt].currCol >> 1);
						}
					}
					}else{
					collisionObjects[collision_cnt].collide = 1;
				}
			}
		}
	}

	if(movementCounter%6==0&&movementCounter!=0){
		movementCounter = 0;
	}

}

void gravity(){
	gravityCounter++;

	if(gravityCounter%3==0&&gravityCounter!=0){
		for(unsigned char collision_cnt = 0; collision_cnt < collisionObjectsUsed; collision_cnt++){
			if(collisionObjects[collision_cnt].id == 0 || collisionObjects[collision_cnt].id == 2){
				if(collisionObjects[collision_cnt].jump_cnt==0){
					if(collisionObjects[collision_cnt].currRow < 0x80){
						collisionObjects[collision_cnt].pastCol = collisionObjects[collision_cnt].currCol;
						collisionObjects[collision_cnt].pastRow = collisionObjects[collision_cnt].currRow;
						collisionObjects[collision_cnt].currRow = (collisionObjects[collision_cnt].currRow << 1);
						collisionObjects[collision_cnt].landed = 0;
						}else{
						collisionObjects[collision_cnt].landed = 1;
					}
					}else{
					if(collisionObjects[collision_cnt].currRow > 0x01){
						collisionObjects[collision_cnt].pastCol = collisionObjects[collision_cnt].currCol;
						collisionObjects[collision_cnt].pastRow = collisionObjects[collision_cnt].currRow;
						collisionObjects[collision_cnt].currRow = (collisionObjects[collision_cnt].currRow >> 1);
					}
					
					if(collisionObjects[collision_cnt].jump_cnt != 0x00){
						collisionObjects[collision_cnt].jump_cnt--;
					}
					
					if(collisionObjects[collision_cnt].currRow == 0x01){
						collisionObjects[collision_cnt].jump_cnt = 0x00;
					}
				}
			}
		}
		gravityCounter = 0;
	}

}

void artificialIntelligence(){

	for(unsigned char cnt_collision = 1; cnt_collision < collisionObjectsUsed; cnt_collision++){
		if(collisionObjects[cnt_collision].id == 2 && collisionObjects[cnt_collision].visible == 1 ){
			if(collisionObjects[cnt_collision].direction == 0){
				collisionObjects[cnt_collision].direction = 1;
			}

			if(collisionObjects[cnt_collision].direction == 1){
				if(collisionObjects[cnt_collision].collide == 0){
					collisionObjects[cnt_collision].left = 1;
					}else{
					collisionObjects[cnt_collision].collide = 0;
					collisionObjects[cnt_collision].direction = 2;
				}
			}

			if(collisionObjects[cnt_collision].direction == 2){
				if(collisionObjects[cnt_collision].collide == 0){
					collisionObjects[cnt_collision].right = 1;
					}else{
					collisionObjects[cnt_collision].collide = 0;
					collisionObjects[cnt_collision].direction = 1;
				}
			}

			if(rand()%5==1){
				if(collisionObjects[cnt_collision].jump_cnt==0x00&&collisionObjects[cnt_collision].landed){
					collisionObjects[cnt_collision].jump_cnt = 3;
					collisionObjects[cnt_collision].landed = 0;
				}
			}

		}
	}

}

void collision(){
	for(unsigned char cnt_collision = 0; cnt_collision < collisionObjectsUsed; cnt_collision++){
		for(unsigned char cnt_other = cnt_collision+1; cnt_other < collisionObjectsUsed; cnt_other++){
			if(collisionObjects[cnt_collision].currCol == collisionObjects[cnt_other].currCol){
				if(collisionObjects[cnt_collision].currRow == collisionObjects[cnt_other].currRow){
					//COLLISION BETWEEN PLAYER/TERRIAN OR ENEMY/TERRIAN
					if((collisionObjects[cnt_collision].id == 0 && collisionObjects[cnt_other].id == 1)||(collisionObjects[cnt_collision].id == 2 && collisionObjects[cnt_other].id == 1)){
						if(collisionObjects[cnt_collision].pastRow<collisionObjects[cnt_collision].currRow){
							collisionObjects[cnt_collision].landed = 1;
						}
						if(collisionObjects[cnt_collision].pastRow>collisionObjects[cnt_collision].currRow){
							collisionObjects[cnt_collision].jump_cnt=0x00;
						}
						if(collisionObjects[cnt_collision].pastRow == collisionObjects[cnt_other].currRow){
							collisionObjects[cnt_collision].collide = 1;
						}

						collisionObjects[cnt_collision].currRow = collisionObjects[cnt_collision].pastRow;
						collisionObjects[cnt_collision].currCol = collisionObjects[cnt_collision].pastCol;
						}else if((collisionObjects[cnt_collision].id == 1 && collisionObjects[cnt_other].id == 0)||(collisionObjects[cnt_collision].id == 1 && collisionObjects[cnt_other].id == 2)){
						if(collisionObjects[cnt_other].pastRow<collisionObjects[cnt_other].currRow){
							collisionObjects[cnt_other].landed = 1;
						}
						if(collisionObjects[cnt_other].pastRow>collisionObjects[cnt_other].currRow){
							collisionObjects[cnt_other].jump_cnt=0x00;
						}
						if(collisionObjects[cnt_collision].currRow == collisionObjects[cnt_other].pastRow){
							collisionObjects[cnt_other].collide = 1;
						}
						collisionObjects[cnt_other].currRow = collisionObjects[cnt_other].pastRow;
						collisionObjects[cnt_other].currCol = collisionObjects[cnt_other].pastCol;
					}
					//COLLISION BETWEEN PLAYER/POINT
					if(collisionObjects[cnt_collision].id == 0 && collisionObjects[cnt_other].id == 3 && collisionObjects[cnt_other].visible == 1){
						scoreFlag = 1;
						collisionObjects[cnt_other].visible = 0;
					}
					//COLLISION BETWEEN ENEMY AND PLAYER
					if(collisionObjects[cnt_collision].id == 0 && collisionObjects[cnt_other].id == 2 && collisionObjects[cnt_other].visible == 1){
						if(collisionObjects[cnt_collision].pastRow<collisionObjects[cnt_collision].currRow){
							collisionObjects[cnt_other].visible = 0;
							}else{
							deathFlag = 1;
							motorFlag = 1;
						}
						}else if(collisionObjects[cnt_collision].id == 2 && collisionObjects[cnt_other].id == 0 && collisionObjects[cnt_collision].visible == 1){
						if(collisionObjects[cnt_collision].pastRow>collisionObjects[cnt_collision].currRow){
							collisionObjects[cnt_collision].visible = 0;
							}else{
							deathFlag = 1;
							motorFlag = 1;
						}
					}
				}
			}
		}
	}
}

void drawObjects(unsigned char cnt_draw){
	if(collisionObjects[cnt_draw-1].visible == 1){
		if(collisionObjects[cnt_draw-1].color == 0x00){
			drawGreen(collisionObjects[cnt_draw-1].currRow, collisionObjects[cnt_draw-1].currCol);
		}
		if(collisionObjects[cnt_draw-1].color == 0x01){
			drawRed(collisionObjects[cnt_draw-1].currRow, collisionObjects[cnt_draw-1].currCol);
		}
		if(collisionObjects[cnt_draw-1].color == 0x02){
			drawYellow(collisionObjects[cnt_draw-1].currRow, collisionObjects[cnt_draw-1].currCol);
		}
	}
}

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
void SPI_MasterInit(void) {
	DDRB = (1<<DDB4) | (1<<DDB5) | (1<<DDB7);
	SPCR = (1<<SPE) |  (1<<MSTR) | (1<SPR0);
}

unsigned char SPI_Transmit(unsigned char data){
	PORTB = PORTB & 0xEF;
	SPDR = data;
	while( !(SPSR & (1<<SPIF) ));
	PORTB = PORTB | 0x10;
	return SPDR;
}


//==========================================================================================//

//===================================STATE MACHINE FUNCTION PROTOTYPES=======================//

enum TN_States {TN_SMStart, TN_Wait};
int TickFct_Transmission(int state);

enum MX_States { MX_SMStart, MX_Wait};
int TickFct_Matrix(int state);

enum GM_States { GM_SMStart, GM_Wait };
int TickFct_GameLogic(int state);

//===========================================================================================//

//========================================MAIN===============================================//

int main(void)
{
	
	DDRA = 0xFF; PORTA = 0xFF;	//OUTPUT
	DDRC = 0xFF; PORTC = 0x00;	//OUTPUT
	DDRD = 0xFF; PORTD = 0x00;	//OUTPUT

	unsigned char i=0;
	tasks[i].state = TN_SMStart;
	tasks[i].period = periodTransmission;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Transmission;
	i++;
	tasks[i].state = MX_SMStart;
	tasks[i].period = periodMatrix;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Matrix;
	i++;
	tasks[i].state = GM_SMStart;
	tasks[i].period = periodGameLogic;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_GameLogic;

	collisionObjects[0].currCol = playerCol;			//Player
	collisionObjects[0].currRow = playerRow;
	collisionObjects[0].pastCol = playerCol;
	collisionObjects[0].pastRow = playerRow;
	collisionObjects[0].spawnCol = playerCol;
	collisionObjects[0].spawnRow = playerRow;
	collisionObjects[0].color = 0x00;
	collisionObjects[0].id = 0;
	collisionObjects[0].visible = 1;
	collisionObjects[0].jump_cnt = 0x00;
	collisionObjects[0].left = 0x00;
	collisionObjects[0].right = 0x00;
	collisionObjects[0].direction = 0x00;
	collisionObjects[0].changeDir = 0x00;
	collisionObjects[0].spawned = 0;
	collisionObjects[0].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[1].currCol = 0x01;					//Terrain				0x01:0x80
	collisionObjects[1].currRow = 0x80;
	collisionObjects[1].pastCol = 0x01;
	collisionObjects[1].pastRow = 0x80;
	collisionObjects[1].spawnCol = 0x01;
	collisionObjects[1].spawnRow = 0x80;
	collisionObjects[1].color = 0x00;
	collisionObjects[1].id = 1;
	collisionObjects[1].visible = 1;
	collisionObjects[1].jump_cnt = 0x00;
	collisionObjects[1].left = 0x00;
	collisionObjects[1].right = 0x00;
	collisionObjects[1].direction = 0x00;
	collisionObjects[1].changeDir = 0x00;
	collisionObjects[1].spawned = 0;
	collisionObjects[1].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[2].currCol = 0x80;					//Terrain				0x80:0x80
	collisionObjects[2].currRow = 0x80;
	collisionObjects[2].pastCol = 0x80;
	collisionObjects[2].pastRow = 0x80;
	collisionObjects[2].spawnCol = 0x80;
	collisionObjects[2].spawnRow = 0x80;
	collisionObjects[2].color = 0x00;
	collisionObjects[2].id = 1;
	collisionObjects[2].visible = 1;
	collisionObjects[2].jump_cnt = 0x00;
	collisionObjects[2].left = 0x00;
	collisionObjects[2].right = 0x00;
	collisionObjects[2].direction = 0x00;
	collisionObjects[2].changeDir = 0x00;
	collisionObjects[2].spawned = 0;
	collisionObjects[2].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[3].currCol = 0x40;					//Terrain				0x40:0x80
	collisionObjects[3].currRow = 0x80;
	collisionObjects[3].pastCol = 0x40;
	collisionObjects[3].pastRow = 0x80;
	collisionObjects[3].spawnCol = 0x40;
	collisionObjects[3].spawnRow = 0x80;
	collisionObjects[3].color = 0x00;
	collisionObjects[3].id = 1;
	collisionObjects[3].visible = 1;
	collisionObjects[3].jump_cnt = 0x00;
	collisionObjects[3].left = 0x00;
	collisionObjects[3].right = 0x00;
	collisionObjects[3].direction = 0x00;
	collisionObjects[3].changeDir = 0x00;
	collisionObjects[3].spawned = 0;
	collisionObjects[3].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[4].currCol = 0x80;					//Terrain				0x80:0x40
	collisionObjects[4].currRow = 0x40;
	collisionObjects[4].pastCol = 0x80;
	collisionObjects[4].pastRow = 0x40;
	collisionObjects[4].spawnCol = 0x80;
	collisionObjects[4].spawnRow = 0x40;
	collisionObjects[4].color = 0x00;
	collisionObjects[4].id = 1;
	collisionObjects[4].visible = 1;
	collisionObjects[4].jump_cnt = 0x00;
	collisionObjects[4].left = 0x00;
	collisionObjects[4].right = 0x00;
	collisionObjects[4].direction = 0x00;
	collisionObjects[4].changeDir = 0x00;
	collisionObjects[4].spawned = 0;
	collisionObjects[4].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[5].currCol = 0x08;					//Terrain				0x08:0x20
	collisionObjects[5].currRow = 0x20;
	collisionObjects[5].pastCol = 0x08;
	collisionObjects[5].pastRow = 0x20;
	collisionObjects[5].spawnCol = 0x08;
	collisionObjects[5].spawnRow = 0x20;
	collisionObjects[5].color = 0x00;
	collisionObjects[5].id = 1;
	collisionObjects[5].visible = 1;
	collisionObjects[5].jump_cnt = 0x00;
	collisionObjects[5].left = 0x00;
	collisionObjects[5].right = 0x00;
	collisionObjects[5].direction = 0x00;
	collisionObjects[5].changeDir = 0x00;
	collisionObjects[5].spawned = 0;
	collisionObjects[5].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[6].currCol = 0x04;					//Terrain				0x04:0x20
	collisionObjects[6].currRow = 0x20;
	collisionObjects[6].pastCol = 0x04;
	collisionObjects[6].pastRow = 0x20;
	collisionObjects[6].spawnCol = 0x04;
	collisionObjects[6].spawnRow = 0x20;
	collisionObjects[6].color = 0x00;
	collisionObjects[6].id = 1;
	collisionObjects[6].visible = 1;
	collisionObjects[6].jump_cnt = 0x00;
	collisionObjects[6].left = 0x00;
	collisionObjects[6].right = 0x00;
	collisionObjects[6].direction = 0x00;
	collisionObjects[6].changeDir = 0x00;
	collisionObjects[6].spawned = 0;
	collisionObjects[6].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[7].currCol = 0x80;					//Terrain				0x80:0x08
	collisionObjects[7].currRow = 0x08;
	collisionObjects[7].pastCol = 0x80;
	collisionObjects[7].pastRow = 0x08;
	collisionObjects[7].spawnCol = 0x80;
	collisionObjects[7].spawnRow = 0x08;
	collisionObjects[7].color = 0x00;
	collisionObjects[7].id = 1;
	collisionObjects[7].visible = 1;
	collisionObjects[7].jump_cnt = 0x00;
	collisionObjects[7].left = 0x00;
	collisionObjects[7].right = 0x00;
	collisionObjects[7].direction = 0x00;
	collisionObjects[7].changeDir = 0x00;
	collisionObjects[7].spawned = 0;
	collisionObjects[7].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[8].currCol = 0x40;					//Terrain				0x40:0x08
	collisionObjects[8].currRow = 0x08;
	collisionObjects[8].pastCol = 0x40;
	collisionObjects[8].pastRow = 0x08;
	collisionObjects[8].spawnCol = 0x40;
	collisionObjects[8].spawnRow = 0x08;
	collisionObjects[8].color = 0x00;
	collisionObjects[8].id = 1;
	collisionObjects[8].visible = 1;
	collisionObjects[8].jump_cnt = 0x00;
	collisionObjects[8].left = 0x00;
	collisionObjects[8].right = 0x00;
	collisionObjects[8].direction = 0x00;
	collisionObjects[8].changeDir = 0x00;
	collisionObjects[8].spawned = 0;
	collisionObjects[8].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[9].currCol = 0x20;					//Terrain				0x20:0x08
	collisionObjects[9].currRow = 0x08;
	collisionObjects[9].pastCol = 0x20;
	collisionObjects[9].pastRow = 0x08;
	collisionObjects[9].spawnCol = 0x20;
	collisionObjects[9].spawnRow = 0x08;
	collisionObjects[9].color = 0x00;
	collisionObjects[9].id = 1;
	collisionObjects[9].visible = 1;
	collisionObjects[9].jump_cnt = 0x00;
	collisionObjects[9].left = 0x00;
	collisionObjects[9].right = 0x00;
	collisionObjects[9].direction = 0x00;
	collisionObjects[9].changeDir = 0x00;
	collisionObjects[9].spawned = 0;
	collisionObjects[9].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[10].currCol = 0x01;					//Terrain			0x01:0x08
	collisionObjects[10].currRow = 0x08;
	collisionObjects[10].pastCol = 0x01;
	collisionObjects[10].pastRow = 0x08;
	collisionObjects[10].spawnCol = 0x01;
	collisionObjects[10].spawnRow = 0x08;
	collisionObjects[10].color = 0x00;
	collisionObjects[10].id = 1;
	collisionObjects[10].visible = 1;
	collisionObjects[10].jump_cnt = 0x00;
	collisionObjects[10].left = 0x00;
	collisionObjects[10].right = 0x00;
	collisionObjects[10].direction = 0x00;
	collisionObjects[10].changeDir = 0x00;
	collisionObjects[10].spawned = 0;
	collisionObjects[10].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[11].currCol = 0x40;					//Terrain			0x40:0x02
	collisionObjects[11].currRow = 0x02;
	collisionObjects[11].pastCol = 0x40;
	collisionObjects[11].pastRow = 0x02;
	collisionObjects[11].spawnCol = 0x40;
	collisionObjects[11].spawnRow = 0x02;
	collisionObjects[11].color = 0x00;
	collisionObjects[11].id = 1;
	collisionObjects[11].visible = 1;
	collisionObjects[11].jump_cnt = 0x00;
	collisionObjects[11].left = 0x00;
	collisionObjects[11].right = 0x00;
	collisionObjects[11].direction = 0x00;
	collisionObjects[11].changeDir = 0x00;
	collisionObjects[11].spawned = 0;
	collisionObjects[11].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[12].currCol = 0x08;					//Terrain			0x08:0x02
	collisionObjects[12].currRow = 0x02;
	collisionObjects[12].pastCol = 0x08;
	collisionObjects[12].pastRow = 0x02;
	collisionObjects[12].spawnCol = 0x08;
	collisionObjects[12].spawnRow = 0x02;
	collisionObjects[12].color = 0x00;
	collisionObjects[12].id = 1;
	collisionObjects[12].visible = 1;
	collisionObjects[12].jump_cnt = 0x00;
	collisionObjects[12].left = 0x00;
	collisionObjects[12].right = 0x00;
	collisionObjects[12].direction = 0x00;
	collisionObjects[12].changeDir = 0x00;
	collisionObjects[12].spawned = 0;
	collisionObjects[12].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[13].currCol = 0x04;					//Terrain			0x04:0x02
	collisionObjects[13].currRow = 0x02;
	collisionObjects[13].pastCol = 0x04;
	collisionObjects[13].pastRow = 0x02;
	collisionObjects[13].spawnCol = 0x04;
	collisionObjects[13].spawnRow = 0x02;
	collisionObjects[13].color = 0x00;
	collisionObjects[13].id = 1;
	collisionObjects[13].visible = 1;
	collisionObjects[13].jump_cnt = 0x00;
	collisionObjects[13].left = 0x00;
	collisionObjects[13].right = 0x00;
	collisionObjects[13].direction = 0x00;
	collisionObjects[13].changeDir = 0x00;
	collisionObjects[13].spawned = 0;
	collisionObjects[13].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[14].currCol = 0x80;					//Enemy				0x80:0x20
	collisionObjects[14].currRow = 0x20;
	collisionObjects[14].pastCol = 0x80;
	collisionObjects[14].pastRow = 0x20;
	collisionObjects[14].spawnCol = 0x80;
	collisionObjects[14].spawnRow = 0x20;
	collisionObjects[14].color = 0x01;
	collisionObjects[14].id = 2;
	collisionObjects[14].visible = 1;
	collisionObjects[14].jump_cnt = 0x00;
	collisionObjects[14].left = 0x00;
	collisionObjects[14].right = 0x00;
	collisionObjects[14].direction = 0x00;
	collisionObjects[14].changeDir = 0x00;
	collisionObjects[14].spawned = 0;
	collisionObjects[14].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[15].currCol = 0x08;					//Enemy				0x08:0x01
	collisionObjects[15].currRow = 0x01;
	collisionObjects[15].pastCol = 0x08;
	collisionObjects[15].pastRow = 0x01;
	collisionObjects[15].spawnCol = 0x08;
	collisionObjects[15].spawnRow = 0x01;
	collisionObjects[15].color = 0x01;
	collisionObjects[15].id = 2;
	collisionObjects[15].visible = 1;
	collisionObjects[15].jump_cnt = 0x00;
	collisionObjects[15].left = 0x00;
	collisionObjects[15].right = 0x00;
	collisionObjects[15].direction = 0x00;
	collisionObjects[15].changeDir = 0x00;
	collisionObjects[15].spawned = 0;
	collisionObjects[15].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[16].currCol = 0x40;					//Point				0x40:0x40
	collisionObjects[16].currRow = 0x40;
	collisionObjects[16].pastCol = 0x40;
	collisionObjects[16].pastRow = 0x40;
	collisionObjects[16].spawnCol = 0x40;
	collisionObjects[16].spawnRow = 0x40;
	collisionObjects[16].color = 0x02;
	collisionObjects[16].id = 3;
	collisionObjects[16].visible = 1;
	collisionObjects[16].jump_cnt = 0x00;
	collisionObjects[16].left = 0x00;
	collisionObjects[16].right = 0x00;
	collisionObjects[16].direction = 0x00;
	collisionObjects[16].changeDir = 0x00;
	collisionObjects[16].spawned = 0;
	collisionObjects[16].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[17].currCol = 0x01;						//Point				0x01:0x40
	collisionObjects[17].currRow = 0x40;
	collisionObjects[17].pastCol = 0x01;
	collisionObjects[17].pastRow = 0x40;
	collisionObjects[17].spawnCol = 0x01;
	collisionObjects[17].spawnRow = 0x40;
	collisionObjects[17].color = 0x02;
	collisionObjects[17].id = 3;
	collisionObjects[17].visible = 1;
	collisionObjects[17].jump_cnt = 0x00;
	collisionObjects[17].left = 0x00;
	collisionObjects[17].right = 0x00;
	collisionObjects[17].direction = 0x00;
	collisionObjects[17].changeDir = 0x00;
	collisionObjects[17].spawned = 0;
	collisionObjects[17].collide = 0;
	collisionObjectsUsed++;									//Point				0x04:0x10
	collisionObjects[18].currCol = 0x04;
	collisionObjects[18].currRow = 0x10;
	collisionObjects[18].pastCol = 0x04;
	collisionObjects[18].pastRow = 0x10;
	collisionObjects[18].spawnCol = 0x04;
	collisionObjects[18].spawnRow = 0x10;
	collisionObjects[18].color = 0x02;
	collisionObjects[18].id = 3;
	collisionObjects[18].visible = 1;
	collisionObjects[18].jump_cnt = 0x00;
	collisionObjects[18].left = 0x00;
	collisionObjects[18].right = 0x00;
	collisionObjects[18].direction = 0x00;
	collisionObjects[18].changeDir = 0x00;
	collisionObjects[18].spawned = 0;
	collisionObjects[18].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[19].currCol = 0x40;						//Point				0x40:0x04
	collisionObjects[19].currRow = 0x04;
	collisionObjects[19].pastCol = 0x40;
	collisionObjects[19].pastRow = 0x04;
	collisionObjects[19].spawnCol = 0x40;
	collisionObjects[19].spawnRow = 0x04;
	collisionObjects[19].color = 0x02;
	collisionObjects[19].id = 3;
	collisionObjects[19].visible = 1;
	collisionObjects[19].jump_cnt = 0x00;
	collisionObjects[19].left = 0x00;
	collisionObjects[19].right = 0x00;
	collisionObjects[19].direction = 0x00;
	collisionObjects[19].changeDir = 0x00;
	collisionObjects[19].spawned = 0;
	collisionObjects[19].collide = 0;
	collisionObjectsUsed++;
	collisionObjects[20].currCol = 0x04;						//Point				0x04:0x01
	collisionObjects[20].currRow = 0x01;
	collisionObjects[20].pastCol = 0x04;
	collisionObjects[20].pastRow = 0x01;
	collisionObjects[20].spawnCol = 0x04;
	collisionObjects[20].spawnRow = 0x01;
	collisionObjects[20].color = 0x02;
	collisionObjects[20].id = 3;
	collisionObjects[20].visible = 1;
	collisionObjects[20].jump_cnt = 0x00;
	collisionObjects[20].left = 0x00;
	collisionObjects[20].right = 0x00;
	collisionObjects[20].direction = 0x00;
	collisionObjects[20].changeDir = 0x00;
	collisionObjects[20].spawned = 0;
	collisionObjects[20].collide = 0;
	collisionObjectsUsed++;

	TimerSet(tasksPeriodGCD);
	TimerOn();
	SPI_MasterInit();
	srand(4562);

	while (1)
	{
		while(!TimerFlag);
		TimerFlag = 0;
	}

	return 0;
}

//===========================================================================================//

//============================STATE MACHINE FUNCTION DEFINITIONS=============================//

int TickFct_Transmission(int state){
	
	unsigned char dataIn = 0;
	unsigned char dataOut = 0;

	switch(state){	//Transitions
		case TN_SMStart:
		state = TN_Wait;
		break;

		case TN_Wait:
		//first bit is motor.
		//second bit is to increment score.
		//third bit is reset from win.
		
		if(motorFlag){
			motorFlag = 0;
			dataOut = dataOut | 0x01;
		}

		if(scoreFlag){
			scoreFlag = 0;
			dataOut = dataOut | 0x02;
		}

		if(winFlag){
			winFlag = 0;
			resetFlag = 1;
			greenOutFlag = 1;
			dataOut = dataOut | 0x04;
		}

		if(deathFlag){
			deathFlag = 0;
			resetFlag = 1;
			redOutFlag = 1;
			dataOut = dataOut | 0x08;
		}

		dataIn = SPI_Transmit(dataOut);

		if((dataIn&0x01)==0x01){	//set rightFlag
			rightFlag = 1;
		}
		if((dataIn&0x02)==0x02){	//set leftFlag
			leftFlag = 1;
		}
		if((dataIn&0x04)==0x04){	//set crouchFlag
			crouchFlag = 1;
		}
		if((dataIn&0x08)==0x08){	//set jumpFlag
			jumpFlag = 1;
		}
		if((dataIn&0x10)==0x10){	//set resetFlag
			resetFlag = 1;
		}
		if((dataIn&0x20)==0x20){	//set winFlag.
			winFlag = 1;
		}

		break;

		default:
		state = TN_SMStart;
		break;
	}

	return state;
}



int TickFct_Matrix(int state){

	// 	unsigned char tempD = 0;
	// 	unsigned char tempC = 0;
	// 	unsigned char print_cnt = 0;
	// 	unsigned char numRow = 0;
	// 	unsigned char givenRow = 0;
	static unsigned long cnt_draw = 1;
	static unsigned long cnt_red = 0;
	static unsigned long cnt_green = 0;

	switch(state){	//Transitions
		case MX_SMStart:
		state = MX_Wait;
		break;
		
		case MX_Wait:
		state = MX_Wait;

		if(redOutFlag){
			redOutFlag = 0;
			cnt_red = 250;
		}

		if(greenOutFlag){
			greenOutFlag = 0;
			cnt_green = 250;
		}

		if(cnt_green != 0){
			cnt_green--;
			PORTD = 0x00;
			PORTC = 0xFF;
			PORTA = 0xFF;
		}

		if(cnt_red != 0){
			cnt_red--;
			PORTD = 0xFF;
			PORTC = 0xFF;
			PORTA = 0x00;

		}

		if(cnt_red == 0 && cnt_green == 0){
			drawObjects(cnt_draw);
			
			if(cnt_draw == collisionObjectsUsed){
				cnt_draw = 0;
			}
			
			cnt_draw++;
		}
		
		break;
		
		default:
		state = MX_SMStart;
		break;
	}

	return state;
}

int TickFct_GameLogic(int state){
	
	// 	unsigned char joystickVal = 0;
	// 	unsigned char jumpVal = 0;
	// 	unsigned char leftVal = 0;
	// 	unsigned char rightVal = 0;
	
	switch(state){	//Transitions
		case GM_SMStart:
		state = GM_Wait;
		gravityCounter = 0;
		break;
		
		case GM_Wait:
		state = GM_Wait;

		playerInput();

		movement();
		
		gravity();
		
		artificialIntelligence();

		collision();

		break;
		
		default:
		state = GM_SMStart;
		break;
	}
	
	return state;
}

//===========================================================================================//
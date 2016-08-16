/*
 * @briefcase alarm locking system, LPC4088 board.
 */


#include <stdbool.h>
#include <ucos_ii.h>
#include <mbed.h>
#include <display.h>
#include <MMA7455.h>
#include "buffer.h"

/********************************************************************************************************
*                                            APPLICATION TASK PRIORITIES
********************************************************************************************************/

typedef enum {
	APP_TASK_BUTTONS_PRIO = 4,
	APP_TASK_LCD_PRIO,
	APP_TASK_POT_PRIO,
	APP_TASK_ACC_PRIO,
  	APP_TASK_LED_PRIO
} taskPriorities_t;

/********************************************************************************************************
*                                            APPLICATION TASK STACKS
********************************************************************************************************/

#define  APP_TASK_BUTTONS_STK_SIZE           256
#define  APP_TASK_POT_STK_SIZE               256
#define  APP_TASK_ACC_STK_SIZE               256
#define  APP_TASK_LCD_STK_SIZE               256
#define  APP_TASK_LED_STK_SIZE               256


static OS_STK appTaskButtonsStk[APP_TASK_BUTTONS_STK_SIZE];
static OS_STK appTaskPotStk[APP_TASK_POT_STK_SIZE];
static OS_STK appTaskAccStk[APP_TASK_ACC_STK_SIZE];
static OS_STK appTaskLcdStk[APP_TASK_LCD_STK_SIZE];
static OS_STK appTaskLedStk[APP_TASK_LED_STK_SIZE];


/********************************************************************************************************
*                                            APPLICATION TASK PROTOTYPES
********************************************************************************************************/

static void appTaskButtons(void *pdata);
static void appTaskPot(void *pdata);
static void appTaskAcc(void *pdata);
static void appTaskLcd(void *pdata);
static void appTaskLed(void *pdata);


/********************************************************************************************************
*                                            GLOBAL TYPES AND VARIABLES 
********************************************************************************************************/

typedef enum {
	JLEFT = 0,
	JRIGHT,
	JUP,
	JDOWN,
	JCENTER
} buttonId_t;

// message IDs
typedef enum messageTypes {
	//States
	M_BRIEFCASE_LOCKED = 0, 
	M_BRIEFCASE_UNLOCKED = 1,
	M_BRIEFCASE_MOVING = 2,
	M_SECURITY_ENABELD,
	M_SECURITY_DISABLED, 
  	M_ALARM_ON, 
	M_ALARM_OFF, 
	M_ALARM_PENDING, 
	M_TIME_INTERVAL, 
	M_COUNTDOWN_VALUE, 
  	M_DISPLAYED_PIN,	
	M_SAVED_PIN, 
  	M_POSITION,
	M_DISPLAY_CLEAR,
	M_PIN_EDIT_ON,
	M_PIN_EDIT_OFF
} messageType_t;

enum {
	FLASH_MIN_DELAY     = 1,
	FLASH_INITIAL_DELAY = 500,
	FLASH_MAX_DELAY     = 1000,
	FLASH_DELAY_STEP    = 50
};

// States
typedef enum { LOCKED, UNLOCKED, MOVING } briefcaseStates;
typedef enum { ENABLED, DISABLED, } securityStates;
typedef enum { ON, OFF, PENDING } alarmStates;
typedef enum { ACTIVE, INACTIVE } pinEditModes;

static briefcaseStates briefcaseState;
static securityStates securityState;
static alarmStates alarmState;
static pinEditModes pinEditMode;

// Inputs
static DigitalIn buttons[] = {P5_0, P5_4, P5_2, P5_1, P5_3}; // LEFT, RIGHT, UP, DOWN, CENTER
static AnalogIn potentiometer(P0_23);
MMA7455 acc(P0_27, P0_28);  //Object to manage the accelerometer

// Outputs
static Display *d = Display::theDisplay();
static DigitalOut led1(P1_18);
static DigitalOut led2(P0_13);
static DigitalOut led3(P1_13);
static DigitalOut led4(P2_19);

// Variables
static int32_t flashingDelay = FLASH_INITIAL_DELAY;  
static int32_t accVal[3];
static float potVal;
static uint8_t intVal;
static uint8_t ALARM_INTERVAL = 10;

// Arrays and Index
uint8_t positionArray[4] = {'-', ' ', ' ', ' '};
uint8_t dPinArray[4] = {'0','0','0','0'};
uint8_t sPinArray[4] = {'1','0','0','0'};
uint8_t index = 0;

/********************************************************************************************************
*                                            APPLICATION FUNCTION PROTOTYPES
********************************************************************************************************/

static bool buttonPressedAndReleased(buttonId_t button);
//static void incDigit(uint8_t* pinArray);
//static void decDigit(uint8_t* pinArray);
bool accInit(MMA7455& acc); //prototype of init routine
bool provePin();
void displayInit(void);
void statesInit(void);
void dPinArrayInit(void);
void dPinArrayClear(void);
void positionArrayInit(void);
void positionArrayClear(void);

/********************************************************************************************************
*                                            GLOBAL FUNCTION DEFINITIONS
********************************************************************************************************/

int main() {	

  /* Initialise the OS */
  OSInit();

  /* Create the tasks */
  OSTaskCreate(appTaskButtons,                               
               (void *)0,
               (OS_STK *)&appTaskButtonsStk[APP_TASK_BUTTONS_STK_SIZE - 1],
               APP_TASK_BUTTONS_PRIO);
  
  OSTaskCreate(appTaskPot,                               
               (void *)0,
               (OS_STK *)&appTaskPotStk[APP_TASK_POT_STK_SIZE - 1],
               APP_TASK_POT_PRIO);
							 
  OSTaskCreate(appTaskAcc,                               
               (void *)0,
               (OS_STK *)&appTaskAccStk[APP_TASK_ACC_STK_SIZE - 1],
               APP_TASK_ACC_PRIO);
							 
	OSTaskCreate(appTaskLcd,                               
               (void *)0,
               (OS_STK *)&appTaskLcdStk[APP_TASK_LCD_STK_SIZE - 1],
               APP_TASK_LCD_PRIO);

	OSTaskCreate(appTaskLed,                               
               (void *)0,
               (OS_STK *)&appTaskLedStk[APP_TASK_LED_STK_SIZE - 1],
               APP_TASK_LED_PRIO);
	
	// Initialise the buffer
	bufferSaveInit();
	// Initialise accelerometer
	accInit(acc);
  
  /* Start the OS */
  OSStart();                                                  
  
  /* Should never arrive here */ 
  return 0;      
}

/********************************************************************************************************
*                                            APPLICATION TASK DEFINITIONS
********************************************************************************************************/

/*******************************************************************************************************/
// Button task
/*******************************************************************************************************/
static void appTaskButtons(void *pdata) 
{
  /* Start the OS ticker -- must be done in the highest priority task */
  SysTick_Config(SystemCoreClock / OS_TICKS_PER_SEC); 
	statesInit();
	displayInit();
	
	message_t msg;
	
  /* Task main loop */
  while (true) 
	{
		//---------------------------------------------------------------------------------------------
		// lock briefcase
			if 	(buttonPressedAndReleased(JUP) &&
				(briefcaseState == UNLOCKED) &&
				(securityState == DISABLED) &&
				(alarmState == OFF) &&
				(pinEditMode == INACTIVE) ) 
		{
			briefcaseState = LOCKED;
			msg.taskId = M_BRIEFCASE_LOCKED;
			putBufferSave(&msg);
		}		
		// unlock briefcase
		else if 	(buttonPressedAndReleased(JDOWN) &&
				(briefcaseState == LOCKED) &&
				(securityState == DISABLED) &&
				(alarmState == OFF) &&
				(pinEditMode == INACTIVE) )
		{
			briefcaseState = UNLOCKED;
			msg.taskId = M_BRIEFCASE_UNLOCKED;
			putBufferSave(&msg);
		}
		//---------------------------------------------------------------------------------------------		
		// enable security
    else if (buttonPressedAndReleased(JRIGHT) && 
						(briefcaseState == LOCKED) &&
						(securityState == DISABLED) &&
						(alarmState == OFF) &&
						(pinEditMode == INACTIVE) )
		{
			securityState = ENABLED;
			msg.taskId = M_SECURITY_ENABELD;
			putBufferSave(&msg);
		
			dPinArrayInit();
			msg.taskId = M_DISPLAYED_PIN;
			msg.dataArray[0] = dPinArray[0];
			msg.dataArray[1] = dPinArray[1];
			msg.dataArray[2] = dPinArray[2];
			msg.dataArray[3] = dPinArray[3];
			putBufferSave(&msg);
		
			positionArrayInit();
			msg.taskId = M_POSITION;
			msg.dataArray[0] = positionArray[0];
			msg.dataArray[1] = positionArray[1];
			msg.dataArray[2] = positionArray[2];
			msg.dataArray[3] = positionArray[3];
			putBufferSave(&msg);				
		}
		// disable security 
		else if (buttonPressedAndReleased(JCENTER) && 
						(briefcaseState == LOCKED || MOVING) && 
						(securityState == ENABLED) &&
						(alarmState == OFF || PENDING || ON ) && 
						(pinEditMode == INACTIVE)	)
		{	
			if (provePin())
			{
				briefcaseState = LOCKED;
				msg.taskId = M_BRIEFCASE_LOCKED;
				putBufferSave(&msg);
			
				securityState = DISABLED;
				msg.taskId = M_SECURITY_DISABLED;
				putBufferSave(&msg);
			
				alarmState = OFF;
				msg.taskId = M_ALARM_OFF;
				putBufferSave(&msg);
						
				msg.taskId = M_DISPLAY_CLEAR;
				putBufferSave(&msg);
			}
		}
		
		// increase dispayedPin digit
		else if (buttonPressedAndReleased(JUP) && 
						(briefcaseState == LOCKED || MOVING)&& 
						(securityState == ENABLED) &&
						(alarmState == OFF || PENDING || ON) && 
						(pinEditMode == INACTIVE) )
		{
			if (dPinArray[index] < '9') dPinArray[index] += 1;
			else dPinArray[index] = '0';
			msg.taskId = M_DISPLAYED_PIN;
			msg.dataArray[0] = dPinArray[0];
			msg.dataArray[1] = dPinArray[1];
			msg.dataArray[2] = dPinArray[2];
			msg.dataArray[3] = dPinArray[3];
			putBufferSave(&msg);		
		}
		// decrese displayedPin digit
		else if (buttonPressedAndReleased(JDOWN) && 
						(briefcaseState == LOCKED || MOVING) && 
						(securityState == ENABLED) &&
						(alarmState == OFF || PENDING || ON) &&
						(pinEditMode == INACTIVE) )
		{
			if (dPinArray[index] > '0') dPinArray[index] -= 1;
			else dPinArray[index] = '9';
			msg.taskId = M_DISPLAYED_PIN;
			msg.dataArray[0] = dPinArray[0];
			msg.dataArray[1] = dPinArray[1];
			msg.dataArray[2] = dPinArray[2];
			msg.dataArray[3] = dPinArray[3];
			putBufferSave(&msg);
		}
		// displayedPin digit left
		else if (buttonPressedAndReleased(JRIGHT) && 
						(briefcaseState == LOCKED || UNLOCKED || MOVING) && 
						(securityState == ENABLED ) &&
						(alarmState == OFF || PENDING || ON) &&
						(pinEditMode == INACTIVE) )
		{
			// remove '-' old position
			positionArray[index] = ' ';
			// set '-' new position
			if ( index > 0) index = ( index - 1 ) % 4;
			else index=3;
			
			positionArray[index] = '-';
			// sent msg
		
			msg.taskId = M_POSITION;
			msg.dataArray[0] = positionArray[0];
			msg.dataArray[1] = positionArray[1];
			msg.dataArray[2] = positionArray[2];
			msg.dataArray[3] = positionArray[3];
			putBufferSave(&msg);
		}
		// displayedPin digit right
		else if (buttonPressedAndReleased(JLEFT) && 
						(briefcaseState == LOCKED || UNLOCKED || MOVING) && 
						(securityState == ENABLED ) &&
						(alarmState == OFF || PENDING || ON) &&
						(pinEditMode == INACTIVE) )
		{
			// remove '-' old position
			positionArray[index] = ' ';
			// set '-' new position
			index = ( index + 1 ) % 4;
			positionArray[index] = '-';
		
			msg.taskId = M_POSITION;
			msg.dataArray[0] = positionArray[0];
			msg.dataArray[1] = positionArray[1];
			msg.dataArray[2] = positionArray[2];
			msg.dataArray[3] = positionArray[3];
			putBufferSave(&msg);			
		}
		//---------------------------------------------------------------------------------------------
		// enter pinEditMode
		else if (buttonPressedAndReleased(JLEFT) &&
						(briefcaseState == LOCKED || UNLOCKED) &&
						(securityState == DISABLED) &&
						(alarmState == OFF) &&
						(pinEditMode == INACTIVE) ) 
		{
			pinEditMode = ACTIVE;
			msg.taskId = M_PIN_EDIT_ON;
			putBufferSave(&msg);
		
			msg.taskId = M_DISPLAYED_PIN;
			msg.dataArray[0] = sPinArray[0];
			msg.dataArray[1] = sPinArray[1];
			msg.dataArray[2] = sPinArray[2];
			msg.dataArray[3] = sPinArray[3];
			putBufferSave(&msg);
		
			positionArrayInit();
			msg.taskId = M_POSITION;
			msg.dataArray[0] = positionArray[0];
			msg.dataArray[1] = positionArray[1];
			msg.dataArray[2] = positionArray[2];
			msg.dataArray[3] = positionArray[3];
			putBufferSave(&msg);
		}		
		
		// exit pinEditMode
    else if (buttonPressedAndReleased(JCENTER) && 
						(securityState == DISABLED) )
		{
			pinEditMode = INACTIVE;
			msg.taskId = M_PIN_EDIT_OFF;
			putBufferSave(&msg);
			
			msg.taskId = M_DISPLAY_CLEAR;
			putBufferSave(&msg);
		}
			
		// increase savedPin digit
		else if (buttonPressedAndReleased(JUP) &&
						(pinEditMode == ACTIVE) && 
						(securityState == DISABLED) )
		{
			if (sPinArray[index] < '9') sPinArray[index] += 1;
			else sPinArray[index] = '0';
			msg.taskId = M_DISPLAYED_PIN;
			msg.dataArray[0] = sPinArray[0];
			msg.dataArray[1] = sPinArray[1];
			msg.dataArray[2] = sPinArray[2];
			msg.dataArray[3] = sPinArray[3];
			putBufferSave(&msg);
		}
			
		// decrese savedPin digit
		else if (buttonPressedAndReleased(JDOWN) &&
						(pinEditMode == ACTIVE) && 
						(securityState == DISABLED) )
		{
			if (sPinArray[index] > '0') sPinArray[index] -= 1;
			else sPinArray[index] = '9';
			msg.taskId = M_DISPLAYED_PIN;
			msg.dataArray[0] = sPinArray[0];
			msg.dataArray[1] = sPinArray[1];
			msg.dataArray[2] = sPinArray[2];
			msg.dataArray[3] = sPinArray[3];
			putBufferSave(&msg);
			  
		}
		
		// savedPin digit left
		else if (buttonPressedAndReleased(JRIGHT) && 
						(pinEditMode == ACTIVE) && 
						(securityState == DISABLED) )
		{
			// remove '-' old position
			positionArray[index] = ' ';
			// set '-' new position
			if ( index > 0) index = ( index - 1 ) % 4;
			else index=3;
			
			positionArray[index] = '-';
			
			// sent msg
			msg.taskId = M_POSITION;
			msg.dataArray[0] = positionArray[0];
			msg.dataArray[1] = positionArray[1];
			msg.dataArray[2] = positionArray[2];
			msg.dataArray[3] = positionArray[3];
			putBufferSave(&msg);
		}
		// savedPin digit right
		else if (buttonPressedAndReleased(JLEFT) && 
						(pinEditMode == ACTIVE) && 
						(securityState == DISABLED) )
		{
			// remove '-' old position
			positionArray[index] = ' ';
			// set '-' new position
			index = ( index + 1 ) % 4;
			positionArray[index] = '-';
		
			msg.taskId = M_POSITION;
			msg.dataArray[0] = positionArray[0];
			msg.dataArray[1] = positionArray[1];
			msg.dataArray[2] = positionArray[2];
			msg.dataArray[3] = positionArray[3];
			putBufferSave(&msg);	
		}

		//---------------------------------------------------------------------------------------------
    OSTimeDlyHMSM(0,0,0,100);
  }
}

// Potentiometer task
/*******************************************************************************************************/
static void appTaskPot(void *pdata) {
	message_t msg;
	
  while (true) 
	{
		if ((briefcaseState == LOCKED || UNLOCKED) && 
				(securityState == DISABLED) &&
				(alarmState == OFF) &&
				(pinEditMode == INACTIVE) )
		{
			potVal = (120)*potentiometer.read();
			intVal = (uint8_t)potVal;
			if(intVal >= 10 && intVal <=120)
			{
				ALARM_INTERVAL = intVal;
				
				msg.taskId = M_TIME_INTERVAL;
				msg.dataArray[0] = ALARM_INTERVAL;
				msg.dataArray[1] = ALARM_INTERVAL;
				putBufferSave(&msg);
			}			
		}
		else if (briefcaseState == MOVING && 
						(securityState == ENABLED) &&
						(alarmState == PENDING) &&
						(pinEditMode == INACTIVE) )
		{
			ALARM_INTERVAL -=1;
			
			msg.taskId = M_COUNTDOWN_VALUE;
			msg.dataArray[0] = ALARM_INTERVAL;
			putBufferSave(&msg);
			
			if (ALARM_INTERVAL == 0)
			{
				alarmState = ON;
				msg.taskId = M_ALARM_ON;
				putBufferSave(&msg);
			}
			OSTimeDlyHMSM(0,0,0,900);
		}	
    OSTimeDlyHMSM(0,0,0,100);	
  }
}

// Accelerometer task
/*******************************************************************************************************/
static void appTaskAcc(void *pdata) {
	message_t msg;
	
  while (true) 
	{
		if	(briefcaseState == LOCKED && 
				(securityState == ENABLED) &&
				(alarmState == OFF) &&
				(pinEditMode == INACTIVE) )
		{ 
			acc.read(accVal[0], accVal[1], accVal[2]);		
			for (uint32_t i=0; i<3; i++)
			{
				if(accVal[i] >= 40 || accVal[i] <= -40) 
				{
					alarmState = PENDING;
					msg.taskId = M_ALARM_PENDING;
					putBufferSave(&msg);
					briefcaseState = MOVING;
					msg.taskId = M_BRIEFCASE_MOVING;
					putBufferSave(&msg);
				} 
			}
		}
		OSTimeDlyHMSM(0,0,0,200);
	}
}

// LCD task
/*******************************************************************************************************/
static void appTaskLcd(void *pdata) {
	/* Initialise the display */	
	d->fillScreen(BLACK);
	d->setTextColor(GREEN, BLACK);
	d->setCursor(5, 5);
	d->printf("EN0572 Assignment");
	d->setCursor(5, 15);
	d->printf("Paris Wegener");
	
	uint32_t x = 150;
	uint32_t y = 30;
	
	// Frame
	d->drawRect(x + 10, y + 30, 150, 140, GREEN);
	
	message_t msg;
	while(true)
	{
	
		getBufferSave(&msg);
		
		// Info
		switch(msg.taskId)
		{
			// Security states
			case M_SECURITY_DISABLED:        	
				d->setCursor(x + 20, y + 40);	d->printf("Security   : OFF    "); break;
			case M_SECURITY_ENABELD:        	
				d->setCursor(x + 20, y + 40);	d->printf("Security   : ON     "); break;
			
			// Alarm states
			case M_ALARM_ON:        	
				d->setCursor(x + 20, y + 55);	d->printf("Alarm      : ON     "); break;
			case M_ALARM_OFF:					
				d->setCursor(x + 20, y + 55);	d->printf("Alarm      : OFF    "); break; 
			case M_ALARM_PENDING:			
				d->setCursor(x + 20, y + 55);	d->printf("Alarm      : PENDING"); break; 
			
			// Time interval / countdown
		  case M_TIME_INTERVAL:			
				d->setCursor(x + 20, y + 70);	d->printf("Interval   : %d ", msg.dataArray[0]); 
				d->setCursor(x + 20, y + 85);	d->printf("Time       : %d ", msg.dataArray[1]); break;
			case M_COUNTDOWN_VALUE:		
				d->setCursor(x + 20, y + 85); d->printf("Time       : %d ", msg.dataArray[1]); break;
			
			// Briefcase states
		  case M_BRIEFCASE_UNLOCKED:		
				d->setCursor(x + 20, y + 100); d->printf("Case       : UNLOCKED"); 
				d->setCursor(x + 20, y + 115); d->printf("                     "); break;
			case M_BRIEFCASE_LOCKED:	
				d->setCursor(x + 20, y + 100); d->printf("Case       : LOCKED  "); 
				d->setCursor(x + 20, y + 115); d->printf("                     "); break;
			case M_BRIEFCASE_MOVING:  
				d->setCursor(x + 20, y + 115); d->printf("             MOVING  "); break;
			
			// Code display
			case M_DISPLAYED_PIN:
				d->setCursor(x + 20, y + 130); d->printf("Code       : %c %c %c %c", 
				msg.dataArray[0], msg.dataArray[1], msg.dataArray[2], msg.dataArray[3]); break;
			
			// Clear Display
			case M_DISPLAY_CLEAR:
				d->setCursor(x + 20, y + 130); d->printf("                     ");
				d->setCursor(x + 20, y + 140); d->printf("                     ");
				d->setCursor(x + 20, y + 150); d->printf("                     "); break;

			// Current digit
			case M_POSITION:
				d->setCursor(x + 20, y + 140); d->printf("             %c %c %c %c",
				msg.dataArray[0], msg.dataArray[1], msg.dataArray[2], msg.dataArray[3]); break;
			
			// Edit Mode
			case M_PIN_EDIT_ON:
				d->setCursor(x + 20, y + 150); d->printf("             Edit Pin"); break;			
		}		
		OSTimeDlyHMSM(0,0,0,100);
	}	
}

// LED task
/*******************************************************************************************************/
static void appTaskLed(void *pdata) {	
  while (true) {
		if (alarmState == ON) {
      led1 = !led1;
			led2 = !led2;
			led3 = !led3;
			led4 = !led4;
		}
    OSTimeDly(flashingDelay);
  }
}

// Funktions
/*******************************************************************************************************/
/*
 * @brief buttonPressedAndReleased(button) tests to see if the button has
 *        been pressed then released.
 *        
 * @param button - the name of the button
 * @result - true if button pressed then released, otherwise false
 *
 * If the value of the button's pin is 0 then the button is being pressed,
 * just remember this in savedState.
 * If the value of the button's pin is 1 then the button is released, so
 * if the savedState of the button is 0, then the result is true, otherwise
 * the result is false.
 */

/*******************************************************************************************************/
bool buttonPressedAndReleased(buttonId_t b) {
//	bool result = false;
//	static uint32_t savedState[5] = {1,1,1,1,1};
//	uint32_t state;
//	state = buttons[b].read();
//  if ((savedState[b] == 1) && (state == 0)) {
//		result = true;
//	}
//	savedState[b] = state;
//	return result;
	if(buttons[b].read()== 0) { OSTimeDlyHMSM(0,0,0,100); return true;}
	else return false;
}

///*******************************************************************************************************/
//void incDigit(uint32_t* pinArray) {	
//	if (*pinArray + 1 > '9') {
//		*pinArray = '0';
//	}
//	else {
//		*pinArray += 1;
//	}
//}

///*******************************************************************************************************/
//void decDigit(uint8_t* pinArray) {	
//	if (*pinArray - 1 < '0') {
//		*pinArray = '9';
//	}
//	else {
//		*pinArray -= 1;
//	}
//}


/*******************************************************************************************************/
bool accInit(MMA7455& acc) {
  bool result = true;
  if (!acc.setMode(MMA7455::ModeMeasurement)) {
    // screen->printf("Unable to set mode for MMA7455!\n");
    result = false;
  }
  if (!acc.calibrate()) {
    // screen->printf("Failed to calibrate MMA7455!\n");
    result = false;
  }
  // screen->printf("MMA7455 initialised\n");
  return result;
}

/*******************************************************************************************************/
bool provePin() {
	if ( dPinArray[0] == sPinArray[0] &&
			 dPinArray[1] == sPinArray[1] &&
			 dPinArray[2] == sPinArray[2] &&
			 dPinArray[3] == sPinArray[3] ) {return true;}
	else {return false;}
}

void displayInit(void){
	message_t msg;
	
	msg.taskId = M_SECURITY_DISABLED;
	putBufferSave(&msg);

	msg.taskId = M_ALARM_OFF;
	putBufferSave(&msg);
	
	msg.taskId = M_TIME_INTERVAL;
	msg.dataArray[0] = ALARM_INTERVAL;
	msg.dataArray[1] = ALARM_INTERVAL;
	putBufferSave(&msg);
	
	msg.taskId = M_BRIEFCASE_UNLOCKED;
	putBufferSave(&msg);
	
	msg.taskId = M_DISPLAY_CLEAR;
	putBufferSave(&msg);
}

void statesInit(void){
  briefcaseState = UNLOCKED;
	alarmState = OFF;
	securityState = DISABLED;
	pinEditMode = INACTIVE;
}

void dPinArrayInit(void){
	index = 0;
	dPinArray[0] = '0';
	dPinArray[1] = '0';
	dPinArray[2] = '0';
	dPinArray[3] = '0';
}

void dPinArrayClear(void){
	dPinArray[0] = ' ';
	dPinArray[1] = ' ';
	dPinArray[2] = ' ';
	dPinArray[3] = ' ';
}

void positionArrayInit(void){
	index = 0;
	positionArray[0] = '-';
	positionArray[1] = ' ';
	positionArray[2] = ' ';
	positionArray[3] = ' ';
}

void positionArrayClear(void){
	positionArray[0] = ' ';
	positionArray[1] = ' ';
	positionArray[2] = ' ';
	positionArray[3] = ' ';
}



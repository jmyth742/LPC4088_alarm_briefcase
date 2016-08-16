#include <LPC407x_8x_177x_8x.h>
#include "timer.h"

static void (*timer0UserDefinedHandler)();
static void (*timer1UserDefinedHandler)();
static void (*sysTickUserDefinedHandler)();

//void timer0Init(uint32_t tickHz, void (*handler)()) {
//	LPC_SC->PCONP |= (1UL << 1); /* ensure power to TIMER0 */
//	LPC_TIM0->TCR = 0; /* disable the timer during configuration */
//	LPC_TIM0->PR = 0; /* don't scale peripheral clock */
//	LPC_TIM0->CTCR = 0; /* select timer mode, not counter mode */
//	LPC_TIM0->MR0 = PeripheralClock / tickHz - 1; /* set match register for required rate */
//	LPC_TIM0->MCR = 0x03UL; /* interrupt and reset on match */
//	timer0UserDefinedHandler = handler; /* install the user-defined handler */
//	LPC_TIM0->IR = 0x3F; /* reset all TIMER0 interrupts */
//	NVIC_EnableIRQ(TIMER0_IRQn); /* enable the TIMER0 interrupt in the NVIC */
//	LPC_TIM0->TCR |= (1UL << 0); /* enable the timer */
//}

//void timer1Init(uint32_t tickHz, void (*handler)()) {
//	LPC_SC->PCONP |= (1UL << 2); /* ensure power to TIMER1 */
//	LPC_TIM1->TCR = 0; /* disable the timer during configuration */
//	LPC_TIM1->PR = 0; /* don't scale peripheral clock */
//	LPC_TIM1->CTCR = 0; /* select timer mode, not counter mode */
//	LPC_TIM1->MR0 = PeripheralClock / tickHz - 1; /* set match register for required rate */
//	LPC_TIM1->MCR = 0x03UL; /* interrupt and reset on match */
//	timer1UserDefinedHandler = handler; /* install the user-defined handler */
//	LPC_TIM1->IR = 0x3F; /* reset all TIMER1 interrupts */
//	NVIC_EnableIRQ(TIMER1_IRQn); /* enable the TIMER1 interrupt in the NVIC */
//	LPC_TIM1->TCR |= (1UL << 0); /* enable the timer */
//}

/*
 * @brief Configure the SysTick timer to generate an interrupt 
 * @param tickHz - frequency at which to generate the interrupt
 * @param handler - the user-defined handler for the interrupt
 */
void sysTickInit(uint32_t tickHz, void (*handler)()) {
	SysTick->CTRL = 0;                              /* disable timer during configuration */
	SysTick->LOAD = SystemCoreClock / tickHz - 1UL; /* fill the RELOAD register with a value for the rate */
	SysTick->VAL = 0UL;                             /* any value written here resets the value counter to 0 */
	sysTickUserDefinedHandler = handler;
	SysTick->CTRL = 0x07;                           /* enable the counter, enable the interrupt, choose CPU clock */
}

void softTimerInit(softTimer_t *timer, uint32_t tickHz, void (*handler)()) {
	timer->reloadValue = 1000 / tickHz; /* assumes SysTick interrupts at 1kHz */
	timer->count = timer->reloadValue;
	timer->handler = handler;
}

//void TIMER0_IRQHandler(void) {
//	timer0UserDefinedHandler(); /* call the user-defined handler */
//	LPC_TIM0->IR |= (1UL << 0); /* clear the interrupt on MR0 */
//}

//void TIMER1_IRQHandler(void) {
//	timer1UserDefinedHandler(); /* call the user-defined handler */
//	LPC_TIM1->IR |= (1UL << 0); /* clear the interrupt on MR0 */
//}

void SysTick_Handler(void) {
	sysTickUserDefinedHandler();
}


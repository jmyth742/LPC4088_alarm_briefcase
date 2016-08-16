#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>

typedef struct {
	volatile uint32_t count;
	uint32_t reloadValue;
	void (*handler)(void);
} softTimer_t;

void timer0Init(uint32_t tickHz, void (*handler)());
void timer1Init(uint32_t tickHz, void (*handler)());
void sysTickInit(uint32_t tickHz, void (*handler)());
void softTimerInit(softTimer_t *timer, uint32_t tickHz, void (*handler)());

#endif



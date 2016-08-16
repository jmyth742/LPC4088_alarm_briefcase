#include <stdint.h>
#include "buffer.h"
#include <ucos_ii.h>

static message_t buffer [BUF_SIZE];
static uint8_t front = 0;
static uint8_t back = 0;
OS_EVENT *bufMutex;
OS_EVENT *emptySlot;
OS_EVENT *fullSlot;

void bufferSaveInit(void) {
	bufMutex = OSSemCreate(1);
	emptySlot = OSSemCreate(BUF_SIZE);
	fullSlot = OSSemCreate(0);
}

void putBuffer (message_t const * const msg) {
	buffer [back] = *msg;
	back = ( back + 1 ) % BUF_SIZE ;
}

void getBuffer (message_t * const msg) {
	*msg = buffer [front] ;
	front = (front + 1) % BUF_SIZE ;
}

void putBufferSave (message_t const * const msg) {
	uint8_t status;
	
	OSSemPend(emptySlot, 0, &status);
	OSSemPend(bufMutex, 0, &status);
	putBuffer(msg);
	status = OSSemPost(bufMutex);
	status = OSSemPost(fullSlot);
}

void getBufferSave (message_t * const msg) {
	uint8_t status;
	
	OSSemPend(fullSlot, 0, &status);
	OSSemPend(bufMutex, 0, &status);
	getBuffer(msg);
	status = OSSemPost(bufMutex);
	status = OSSemPost(emptySlot);
}

#ifndef __BUFFER_H
#define __BUFFER_H
#include <stdint.h>

enum {
	BUF_SIZE = 4UL
};

typedef struct message {
	uint32_t taskId;
	//uint32_t dataValue;
	uint8_t dataArray[4];	
} message_t ;

void bufferSaveInit(void);
void putBuffer (message_t const * const);
void getBuffer (message_t * const);
void putBufferSave (message_t const * const);
void getBufferSave (message_t * const);

#endif

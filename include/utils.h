#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_FILE_SIZE 256
#define HEAD_MAX 256

/* perror and exit */
#define perror_exit(msg)			\
	do {					\
		perror(msg);			\
		exit(EXIT_FAILURE);		\
	} while(0)

#define error_exit(msg)				\
	do {					\
		fprintf(stderr,msg);		\
		exit(EXIT_FAILURE);		\
	} while(0)

#define BLK_SHIFT 9
#define BYTES_TO_BLKS(byts) ((byts)>>BLK_SHIFT)

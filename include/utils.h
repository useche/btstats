#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#define MAX_FILE_SIZE 256
#define MAX_HEAD 256
#define TENT_OUTS_RQS 32

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

#define DECL_DUP(type,name,src)			\
	type *name = g_new(type,1);		\
	memcpy(name,src,sizeof(type));		\
	

inline static int comp_int(gconstpointer a,gconstpointer b) 
{
	int x = (int)a;
	int y = (int)b;
	
	return x-y;
}

inline static int comp_int64(gconstpointer a,gconstpointer b) 
{
	__u64 x = *((__u64 *)a);
	__u64 y = *((__u64 *)b);
	
	if(x==y)
		return 0;		
	else
		return x>y?1:-1;		
}

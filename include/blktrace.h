#ifndef BLKTRACE_H
#define BLKTRACE_H

#include <stdio.h>
#include <byteswap.h>
#include <endian.h>

#include "blktrace_api.h"

#define MINORBITS	20
#define MINORMASK	((1U << MINORBITS) - 1)
#define MAJOR(dev)	((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)	((unsigned int) ((dev) & MINORMASK))

#define SECONDS(x) 		((unsigned long long)(x) / 1000000000)
#define NANO_SECONDS(x)		((unsigned long long)(x) % 1000000000)
#define DOUBLE_TO_NANO_ULL(d)	((unsigned long long)((d) * 1000000000))

#define t_blks(t)	((t)->bytes >> 9)
#define t_kb(t)		((t)->bytes >> 10)

#define CHECK_MAGIC(t)		(((t)->magic & 0xffffff00) == BLK_IO_TRACE_MAGIC)
#define SUPPORTED_VERSION	(0x07)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define be16_to_cpu(x)		__bswap_16(x)
#define be32_to_cpu(x)		__bswap_32(x)
#define be64_to_cpu(x)		__bswap_64(x)
#define cpu_to_be16(x)		__bswap_16(x)
#define cpu_to_be32(x)		__bswap_32(x)
#define cpu_to_be64(x)		__bswap_64(x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define be16_to_cpu(x)		(x)
#define be32_to_cpu(x)		(x)
#define be64_to_cpu(x)		(x)
#define cpu_to_be16(x)		(x)
#define cpu_to_be32(x)		(x)
#define cpu_to_be64(x)		(x)
#else
#error "Bad arch"
#endif

static inline int verify_trace(struct blk_io_trace *t)
{
	if (!CHECK_MAGIC(t)) {
		fprintf(stderr, "bad trace magic %x\n", t->magic);
		return 1;
	}
	if ((t->magic & 0xff) != SUPPORTED_VERSION) {
		fprintf(stderr, "unsupported trace version %x\n", 
			t->magic & 0xff);
		return 1;
	}

	return 0;
}

/*
 * check whether data is native or not
 */
static inline int check_data_endianness(__u32 magic)
{
	if ((magic & 0xffffff00) == BLK_IO_TRACE_MAGIC) {
		return 1;
	}

	magic = __bswap_32(magic);
	if ((magic & 0xffffff00) == BLK_IO_TRACE_MAGIC) {
		return 0;
	}

	return -1;
}

#endif

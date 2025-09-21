#include <asm/types.h>
#include <plugins.h>

#define DECL_ASSIGN_REQSIZE(name,data)		\
	struct reqsize_data *name = (struct reqsize_data *)data

struct reqsize_data
{
	__u64 min;
	__u64 max;
	__u64 total_size;
	__u64 reqs;
	__u64 reads;
};

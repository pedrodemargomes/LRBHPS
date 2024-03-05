
#ifndef _RESERV_TRACKING_H_
#define _RESERV_TRACKING_H_
#include <linux/mm.h>
#define AGGR_BITMAP_SIZE 4

#define OSA_PF_AGGR 0x1

struct pageblock {
	unsigned long start_pfn; // end_pfn = start_pfn + 512
	struct list_head list;
};

extern wait_queue_head_t osa_hpage_scand_wait;

#endif 

/*
 * Copyright 2006, Red Hat, Inc., Dave Jones
 * Released under the General Public License (GPL).
 *
 * This file contains the linked list validation for DEBUG_LIST.
 */

#include <linux/export.h>
#include <linux/list.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/rculist.h>

#include <linux/mm.h>
/*
 * Check that the data structures for the list manipulations are reasonably
 * valid. Failures here indicate memory corruption (and possibly an exploit
 * attempt).
 */

bool __list_add_valid(struct list_head *new, struct list_head *prev,
		      struct list_head *next)
{
	if (CHECK_DATA_CORRUPTION(prev == NULL,
			"list_add corruption. prev is NULL.\n") ||
	    CHECK_DATA_CORRUPTION(next == NULL,
			"list_add corruption. next is NULL.\n") ||
	    CHECK_DATA_CORRUPTION(next->prev != prev,
			"list_add corruption. next->prev should be prev (%px), but was %px. (next=%px).\n",
			prev, next->prev, next) ||
	    CHECK_DATA_CORRUPTION(prev->next != next,
			"list_add corruption. prev->next should be next (%px), but was %px. (prev=%px).\n",
			next, prev->next, prev) ||
	    CHECK_DATA_CORRUPTION(new == prev || new == next,
			"list_add double add: new=%px, prev=%px, next=%px.\n",
			new, prev, next)) {
		
		struct page *page = list_entry(new, struct page, lru);
		// pr_info("page_to_pfn(page) = %lx", page_to_pfn(page));
		pr_info("page_to_pfn(page) = %lx page_count(page) = %d PageActive(page) = %d PageLRU(page) = %d page->lru = %lx PageTransCompound(page) = %d", page_to_pfn(page), page_count(page), PageActive(page), PageLRU(page), page->lru, test_bit(PG_head, &page->flags) || READ_ONCE(page->compound_head) & 1);
		BUG();

		return false;
	}

	return true;
}
EXPORT_SYMBOL(__list_add_valid);

bool __list_del_entry_valid(struct list_head *entry)
{
	struct list_head *prev, *next;

	prev = entry->prev;
	next = entry->next;

	if (CHECK_DATA_CORRUPTION(next == NULL,
			"list_del corruption, %px->next is NULL\n", entry) ||
	    CHECK_DATA_CORRUPTION(prev == NULL,
			"list_del corruption, %px->prev is NULL\n", entry) ||
	    CHECK_DATA_CORRUPTION(next == LIST_POISON1,
			"list_del corruption, %px->next is LIST_POISON1 (%px)\n",
			entry, LIST_POISON1) ||
	    CHECK_DATA_CORRUPTION(prev == LIST_POISON2,
			"list_del corruption, %px->prev is LIST_POISON2 (%px)\n",
			entry, LIST_POISON2) ||
	    CHECK_DATA_CORRUPTION(prev->next != entry,
			"list_del corruption. prev->next should be %px, but was %px. (prev=%px)\n",
			entry, prev->next, prev) ||
	    CHECK_DATA_CORRUPTION(next->prev != entry,
			"list_del corruption. next->prev should be %px, but was %px. (next=%px)\n",
			entry, next->prev, next)) {
	
		struct page *page = list_entry(entry, struct page, lru);
		// pr_info("page_to_pfn(page) = %lx", page_to_pfn(page));
		pr_err("page_to_pfn(page) = %lx page_count(page) = %d PageActive(page) = %d PageLRU(page) = %d page->lru = %lx PageTransCompound(page) = %d\n\n", page_to_pfn(page), page_count(page), PageActive(page), PageLRU(page), page->lru, test_bit(PG_head, &page->flags) || READ_ONCE(page->compound_head) & 1);
		BUG();

		return false;
	}

	return true;

}
EXPORT_SYMBOL(__list_del_entry_valid);

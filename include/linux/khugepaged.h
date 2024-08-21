/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KHUGEPAGED_H
#define _LINUX_KHUGEPAGED_H

#include <linux/sched/coredump.h> /* MMF_VM_HUGEPAGE */
#include <linux/hashtable.h>

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
extern struct attribute_group khugepaged_attr_group;

extern int khugepaged_init(void);
extern void khugepaged_destroy(void);
extern int start_stop_khugepaged(void);
extern void __khugepaged_enter(struct mm_struct *mm);
extern void __khugepaged_exit(struct mm_struct *mm);
extern void khugepaged_enter_vma(struct vm_area_struct *vma,
				 unsigned long vm_flags);
extern void khugepaged_min_free_kbytes_update(void);

struct thp_reservation {
	struct mutex *lock;
	unsigned long haddr;
	struct page *page;
	struct vm_area_struct *vma;
	struct hlist_node node;
	struct list_head lru;
	DECLARE_BITMAP(mask, 512); // unsigned long *
	unsigned int timestamp;
	int nr_unused;
};

struct thp_resvs {
	atomic_t refcnt;
	struct mutex res_hash_lock;
	DECLARE_HASHTABLE(res_hash, 7);
};

#define	mm_thp_reservations(mm)	((mm)->thp_reservations)

extern struct list_lru thp_reservations_lru;

void thp_resvs_new(struct mm_struct *mm);

enum lru_status thp_lru_free_reservation(struct list_head *item,
					 struct list_lru_one *lru,
					 spinlock_t *lock,
					 void *cb_arg);

int promote_huge_page_address(struct mm_struct *mm, unsigned long haddr);

int collapse_pte_mapped_thp(struct mm_struct *mm, unsigned long addr,
			    bool install_pmd);

extern void __thp_resvs_put(struct thp_resvs *r);
static inline void thp_resvs_put(struct thp_resvs *r)
{
	if (r)
		__thp_resvs_put(r);
}

struct thp_reservation *khugepaged_find_reservation(
	struct mm_struct *mm,
	unsigned long address);

void khugepaged_mod_resv_unused(struct mm_struct *mm,
				unsigned long address, int delta);

struct page *khugepaged_get_reserved_page(
	struct mm_struct *mm,
	struct vm_area_struct *vma,
	unsigned long address,
	bool *out,
	bool *retry,
	unsigned int flags);

void khugepaged_reserve(struct mm_struct *mm,
			struct vm_area_struct *vma,
			unsigned long address);

struct thp_reservation *khugepaged_release_reservation(struct mm_struct *mm,
				    unsigned long address);

void khugepaged_free_reservation(struct mm_struct *mm,
					struct thp_reservation *res);

void _khugepaged_reservations_fixup(struct vm_area_struct *src,
				   struct vm_area_struct *dst);

void _khugepaged_move_reservations_adj(struct vm_area_struct *prev,
				      struct vm_area_struct *next, long adjust);

void thp_reservations_mremap(struct mm_struct *mm,
		struct vm_area_struct *vma,
		unsigned long old_addr, struct vm_area_struct *new_vma,
		unsigned long new_addr, unsigned long len,
		bool need_rmap_locks);


#ifdef CONFIG_SHMEM
extern int collapse_pte_mapped_thp(struct mm_struct *mm, unsigned long addr,
				   bool install_pmd);
#else
static inline int collapse_pte_mapped_thp(struct mm_struct *mm,
					  unsigned long addr, bool install_pmd)
{
	return 0;
}
#endif

static inline void khugepaged_fork(struct mm_struct *mm, struct mm_struct *oldmm)
{
	if (test_bit(MMF_VM_HUGEPAGE, &oldmm->flags))
		__khugepaged_enter(mm);
}

static inline void khugepaged_exit(struct mm_struct *mm)
{
	if (test_bit(MMF_VM_HUGEPAGE, &mm->flags))
		__khugepaged_exit(mm);
}
#else /* CONFIG_TRANSPARENT_HUGEPAGE */
static inline void khugepaged_fork(struct mm_struct *mm, struct mm_struct *oldmm)
{
}
static inline void khugepaged_exit(struct mm_struct *mm)
{
}
static inline void khugepaged_enter_vma(struct vm_area_struct *vma,
					unsigned long vm_flags)
{
}
static inline int collapse_pte_mapped_thp(struct mm_struct *mm,
					  unsigned long addr, bool install_pmd)
{
	return 0;
}

static inline void khugepaged_min_free_kbytes_update(void)
{
}

#define	mm_thp_reservations(vma)	NULL

static inline void thp_resvs_fork(struct vm_area_struct *vma,
				  struct vm_area_struct *pvma)
{
}

static inline void __thp_resvs_put(struct thp_resvs *r)
{
}

static inline void thp_resvs_put(struct thp_resvs *r)
{
}

static inline void khugepaged_mod_resv_unused(struct vm_area_struct *vma,
					      unsigned long address, int delta)
{
}

static inline struct page *khugepaged_get_reserved_page(
	struct mm_struct *mm,
	struct vm_area_struct *vma,
	unsigned long address)
{
	return NULL;
}

static inline void khugepaged_reserve(struct mm_struct *mm, 
				   struct vm_area_struct *vma,
			       unsigned long address)
{
}

static inline void khugepaged_release_reservation(struct mm_struct *mm,
				    unsigned long address)
{
}

static inline void _khugepaged_reservations_fixup(struct vm_area_struct *src,
				   struct vm_area_struct *dst)
{
}

static inline void _khugepaged_move_reservations_adj(
				struct vm_area_struct *prev,
				struct vm_area_struct *next, long adjust)
{
}

static inline void thp_reservations_mremap(struct mm_struct *mm,
		struct vm_area_struct *vma,
		unsigned long old_addr, struct vm_area_struct *new_vma,
		unsigned long new_addr, unsigned long len,
		bool need_rmap_locks)
{
}

#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

#endif /* _LINUX_KHUGEPAGED_H */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/console.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
// #include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/mm_inline.h>
#include <linux/huge_mm.h>
#include <linux/page_idle.h>
#include <linux/ksm.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/freezer.h>
#include <linux/compaction.h>
#include <linux/mmzone.h>
#include <linux/node.h>
#include <linux/workqueue.h>
#include <linux/khugepaged.h>
#include <linux/hugetlb.h>
#include <linux/migrate.h>
#include <linux/balloon_compaction.h>
#include <linux/pagevec.h>
#include <linux/random.h>
#include <asm/uaccess.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <linux/jiffies.h>
#include "../../../fs/proc/internal.h" 
#include "../mm/internal.h"
#include <linux/compaction.h>
#include <linux/mmu_notifier.h>
#include <linux/pagewalk.h>

#include <reservation_tracking/reserv_tracking.h>

DECLARE_WAIT_QUEUE_HEAD(osa_hpage_scand_wait);
static struct workqueue_struct *osa_hpage_scand_wq __read_mostly;
static struct work_struct osa_hpage_scan_work;

static unsigned int scan_sleep_millisecs = 5000;
static unsigned int scan_pid = 0;
static unsigned long sleep_expire;

static ssize_t alloc_scan_pid_show(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%u\n", scan_pid);
}

static ssize_t alloc_scan_pid_store(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   const char *buf, size_t count)
{
	long unsigned int pid;
	int err;

	err = kstrtoul(buf, 10, &pid);
	if (err || pid > UINT_MAX)
		return -EINVAL;

	scan_pid = pid;
	return count;
}

static ssize_t alloc_scan_sleep_show(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%u\n", scan_sleep_millisecs);
}

static ssize_t alloc_scan_sleep_store(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned long msecs;
	int err;

	err = kstrtoul(buf, 10, &msecs);
	if (err || msecs > UINT_MAX)
		return -EINVAL;

	scan_sleep_millisecs = msecs;
	sleep_expire = 0;
	wake_up_interruptible(&osa_hpage_scand_wait);
	return count;
}
static struct kobj_attribute alloc_sleep_millisecs_attr =
	__ATTR(scan_sleep_millisecs, 0644, alloc_scan_sleep_show,
	       alloc_scan_sleep_store);

static struct kobj_attribute alloc_scan_pid_attr =
	__ATTR(scan_pid, 0644, alloc_scan_pid_show,
	       alloc_scan_pid_store);

static struct attribute *reserve_tracking_attr[] = {
	&alloc_sleep_millisecs_attr.attr,
	&alloc_scan_pid_attr.attr,
	NULL,
};

struct attribute_group reserve_tracking_attr_group = {
	.attrs = reserve_tracking_attr,
	.name = "reserv_tracking",
};


static int osa_hpage_scand_wait_event(void)
{
	return scan_sleep_millisecs; //&& !list_empty(&osa_hpage_scan_list);
}

static int osa_hpage_scand_has_work(void)
{
	return scan_sleep_millisecs; //&& !list_empty(&osa_hpage_scan_list);
}


static void osa_hpage_scand_wait_work(void) 
{
	if (osa_hpage_scand_has_work()) {
		sleep_expire = jiffies + msecs_to_jiffies(scan_sleep_millisecs);
		wait_event_freezable_timeout(osa_hpage_scand_wait,
				kthread_should_stop() || time_after_eq(jiffies, sleep_expire),
				msecs_to_jiffies(scan_sleep_millisecs));
	}

	wait_event_freezable(osa_hpage_scand_wait, osa_hpage_scand_wait_event());
}

// unsigned int reservations[513];
void osa_hpage_do_scan(void)
{
	int err, i;
	unsigned int timestamp;
	struct list_head *pos, *aux;
	int list_size = 0;
	int num_freed = 0;
	int num_should_free = 0;
	struct shrink_control sc;
	unsigned int order = 9;
	struct alloc_context *ac;
	int split = 0;
	int free_huge_pages;
	sc.nr_to_scan = 999999999;
	sc.nid = 0;
	struct list_lru_one *l;

	// === DEBUG ===
	// memset(reservations, 0, 513*sizeof(unsigned int));
	// =============

	struct list_lru_node *nlru = &(&thp_reservations_lru)->node[0];
// 	spin_lock(&nlru->lock);
// restart:
// 	l = &nlru->lru;
// 	// pr_info("BEGIN list_for_each_safe");
// 	list_for_each_safe(pos, aux, &l->list) {
// 		list_size++;
// 		struct thp_reservation *res = container_of(pos,
// 						   struct thp_reservation,
// 						   lru);
// 		unsigned int t = jiffies_to_msecs(jiffies) - res->timestamp;
// 		if (t > 5000)
// 			num_should_free++;

// 		if (thp_lru_free_reservation(pos, l, &nlru->lock, 5000) == LRU_REMOVED_RETRY) {
// 			assert_spin_locked(&nlru->lock);
// 			nlru->nr_items--;
// 			num_freed++;
// 			goto restart;
// 		}
// 	}
// 	spin_unlock(&nlru->lock);
// 	pr_info("list_size = %d num_freed = %d num_should_free = %d", list_size, num_freed, num_should_free);

	// === DEBUG ===
	
	// pr_info("reservs:");
	spin_lock(&nlru->lock);
restart:
	l = &nlru->lru;
	list_for_each_safe(pos, aux, &l->list) {
		struct thp_reservation *res = container_of(pos,
						   struct thp_reservation,
						   lru);
		//reservations[bitmap_weight(res->mask, 512)]++;
		// pr_info("bitmap_weight(res->mask, 512) = %d res->haddr = %lx", bitmap_weight(res->mask, 512), res->haddr);

		if (PageCompound(res->page) && bitmap_weight(res->mask, 512) < hugepage_promotion_threshold) {
			if (!mutex_trylock(res->lock))
				continue;
			if (PageCompound(res->page) && bitmap_weight(res->mask, 512) < hugepage_promotion_threshold) {
				struct mm_struct *mm = res->vma->vm_mm;
				if (!mmget_not_zero(mm))
					goto err_mmget;

				list_lru_isolate(l, pos);
				spin_unlock(&nlru->lock);

				mmput(mm);

				// struct list_lru_node *nlru = &(&thp_reservations_lru)->node[0];
				// if (cb_arg == NULL)
				// 	pr_info("thp_lru_free_reservation free cb_arg == NULL nlru->nr_items = %ld", nlru->nr_items);

				// pr_info("resv freed thp_lru_free_reservation cb_arg = %d", (int) cb_arg);

				// pr_info("resv freed thp_lru_free_reservation page_to_pfn(page) = %lx page_count(page) = %d PageCompound(page) = %d res->haddr = %lx vma->vm_flags = %lx", page_to_pfn(page), page_count(page), PageCompound(page), res->haddr, res->vma->vm_flags);

				// pr_info("split_huge_page huge page_to_pfn(page) = %lx PageCompound(page) = %d PageLRU(page) = %d", page_to_pfn(res->page), PageCompound(res->page), PageLRU(res->page));
				get_page(res->page);
				if (!trylock_page(res->page))
					goto next;
				/* split_huge_page() removes page from list on success */
				if (!split_huge_page(res->page))
					split++;
				unlock_page(res->page);
			next:
				put_page(res->page);
				// if (unused)
				// 	mod_node_page_state(page_pgdat(page), NR_THP_RESERVED, -unused);
	
				spin_lock(&nlru->lock);
				if (list_empty(pos)) {
					list_add_tail(pos, &l->list);
					/* Set shrinker bit if the first element was added */
					l->nr_items++;
					nlru->nr_items++;
				}
				mutex_unlock(res->lock);

				goto restart;
			}
		err_mmget:
			mutex_unlock(res->lock);			
		}
	
	}
	spin_unlock(&nlru->lock);
	
	//pr_info("reservations: ");
	//for(i=0; i<513; i++)
	// 	if (reservations[i])
	// 		pr_cont("%d: %d ", i, reservations[i]);
	
	// ==============

	// struct pglist_data *pgdata = NODE_DATA(0);
	// struct deferred_split *ds_queue = &pgdata->deferred_split_queue;
	
	// struct zone *z = &NODE_DATA(0)->node_zones[ZONE_NORMAL];
	// pr_info("zone_page_state(zone, NR_FREE_PAGES) = %ld", zone_page_state(z, NR_FREE_PAGES));

	// pr_info("split_huge_len = %ld", ds_queue->split_queue_len);

	// free_huge_pages = pgdata->node_zones[ZONE_NORMAL].free_area[9].nr_free + 2*pgdata->node_zones[ZONE_NORMAL].free_area[10].nr_free +
	// 				pgdata->node_zones[ZONE_DMA32].free_area[9].nr_free + 2*pgdata->node_zones[ZONE_DMA32].free_area[10].nr_free;
	
	// pr_info("BEFORE osa_hpage_do_scan list_size = %d num_freed = %d num_should_free = %d split_queue_len = %ld free_huge_pages = %d", list_size, num_freed, num_should_free, ds_queue->split_queue_len, free_huge_pages);

	// if (free_huge_pages < 50) {
	// 	// pr_info("zone normal = %d", free_huge_pages);
	
	// split = deferred_split_scan_only_asap(NULL, &sc);
	pr_info("split = %d", split);
	
	// 	compact_nodes();
	// 	ds_queue = &pgdata->deferred_split_queue;
	// 	// pr_info("AFTER osa_hpage_do_scan list_size = %d num_freed = %d num_should_free = %d split_queue_len = %ld split = %d", list_size, num_freed, num_should_free, ds_queue->split_queue_len, split);
	// }

	return;
}


static void osa_hpage_scand(struct work_struct *ws)
{
	set_freezable();
	set_user_nice(current, MAX_NICE);

	while(1) {
		osa_hpage_do_scan();
		osa_hpage_scand_wait_work();
	}

	return ;
}

static int start_stop_osa_hpage_scand(void)
{
	int err = 0;

	pr_alert("start_stop_osa_hpage_scand");

	if (!osa_hpage_scand_wq) {
		osa_hpage_scand_wq = create_singlethread_workqueue("osa_hpage_scand");

		if (osa_hpage_scand_wq) {
			//schedule_work(osa_hpage_scan_work);
			INIT_WORK(&osa_hpage_scan_work, osa_hpage_scand);
			queue_work(osa_hpage_scand_wq, &osa_hpage_scan_work);
		}
	}

	wake_up_interruptible(&osa_hpage_scand_wait);

fail:
	return err;
}

int reserve_tracking_init_sysfs(void) {
	int err;

	struct kobject *sysfs_reserve_tracking_kobj = kobject_create_and_add("reserve_tracking", mm_kobj);
	if (unlikely(!sysfs_reserve_tracking_kobj)) {
		pr_err("failed to create reserv tracking kobject\n");
		return -ENOMEM;
	}

	if (unlikely(!sysfs_reserve_tracking_kobj)) {
		pr_err("failed to create reserv tracking sys kobject\n");
		return -ENOMEM;
	}

	err = sysfs_create_group(sysfs_reserve_tracking_kobj, &reserve_tracking_attr_group);
	if (err) {
		pr_err("failed to register reserv tracking sys group\n");
		goto delete_kobj;
	}

	return 0;

delete_kobj:
	kobject_put(sysfs_reserve_tracking_kobj);
	return err;
}

static int __init osa_hugepage_init(void)
{
	int err;
	
	err = start_stop_osa_hpage_scand();
	if (err)
		goto err_sysfs;

	// /* init sysfs */
	err = reserve_tracking_init_sysfs();
	if (err)
		goto err_sysfs;

	return 0;

	/* not need yet */
	// osa_hugepage_exit_sysfs(hugepage_kobj);
err_sysfs:
	return err;
}
late_initcall(osa_hugepage_init);

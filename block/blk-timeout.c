/*
 * Functions related to generic timeout handling of requests.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/fault-inject.h>

#include "blk.h"

#ifdef CONFIG_FAIL_IO_TIMEOUT

static DECLARE_FAULT_ATTR(fail_io_timeout);

static int __init setup_fail_io_timeout(char *str)
{
	return setup_fault_attr(&fail_io_timeout, str);
}
__setup("fail_io_timeout=", setup_fail_io_timeout);

int blk_should_fake_timeout(struct request_queue *q)
{
	if (!test_bit(QUEUE_FLAG_FAIL_IO, &q->queue_flags))
		return 0;

	return should_fail(&fail_io_timeout, 1);
}

static int __init fail_io_timeout_debugfs(void)
{
	return init_fault_attr_dentries(&fail_io_timeout, "fail_io_timeout");
}

late_initcall(fail_io_timeout_debugfs);

ssize_t part_timeout_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);
	int set = test_bit(QUEUE_FLAG_FAIL_IO, &disk->queue->queue_flags);

	return sprintf(buf, "%d\n", set != 0);
}

ssize_t part_timeout_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct gendisk *disk = dev_to_disk(dev);
	int val;

	if (count) {
		struct request_queue *q = disk->queue;
		char *p = (char *) buf;

		val = simple_strtoul(p, &p, 10);
		spin_lock_irq(q->queue_lock);
		if (val)
			queue_flag_set(QUEUE_FLAG_FAIL_IO, q);
		else
			queue_flag_clear(QUEUE_FLAG_FAIL_IO, q);
		spin_unlock_irq(q->queue_lock);
	}

	return count;
}

#endif /* CONFIG_FAIL_IO_TIMEOUT */

/*
 * blk_delete_timer - Delete/cancel timer for a given function.
 * @req:	request that we are canceling timer for
 *
 */
void blk_delete_timer(struct request *req)
{
	struct request_queue *q = req->q;

	list_del_init(&req->timeout_list);
	if (list_empty(&q->timeout_list))
		del_timer(&q->timeout);
}

static void blk_rq_timed_out(struct request *req)
{
	struct request_queue *q = req->q;
	enum blk_eh_timer_return ret;

	ret = q->rq_timed_out_fn(req);
	switch (ret) {
	case BLK_EH_HANDLED:
		__blk_complete_request(req);
		break;
	case BLK_EH_RESET_TIMER:
		blk_clear_rq_complete(req);
		blk_add_timer(req);
		break;
	case BLK_EH_NOT_HANDLED:
		/*
		 * LLD handles this for now but in the future
		 * we can send a request msg to abort the command
		 * and we can move more of the generic scsi eh code to
		 * the blk layer.
		 */
		break;
	default:
		printk(KERN_ERR "block: bad eh return: %d\n", ret);
		break;
	}
}

void blk_rq_timed_out_timer(unsigned long data)
{
	struct request_queue *q = (struct request_queue *) data;
	unsigned long flags, next = 0;
	struct request *rq, *tmp;

	spin_lock_irqsave(q->queue_lock, flags);

	list_for_each_entry_safe(rq, tmp, &q->timeout_list, timeout_list) {
		if (time_after_eq(jiffies, rq->deadline)) {
			list_del_init(&rq->timeout_list);

			/*
			 * Check if we raced with end io completion
			 */
			if (blk_mark_rq_complete(rq))
				continue;
			blk_rq_timed_out(rq);
		} else {
			if (!next || time_after(next, rq->deadline))
				next = rq->deadline;
		}
	}

	/*
	 * next can never be 0 here with the list non-empty, since we always
	 * bump ->deadline to 1 so we can detect if the timer was ever added
	 * or not. See comment in blk_add_timer()
	 */
	if (next)
		mod_timer(&q->timeout, round_jiffies_up(next));

	spin_unlock_irqrestore(q->queue_lock, flags);
}

/**
 * blk_abort_request -- Request request recovery for the specified command
 * @req:	pointer to the request of interest
 *
 * This function requests that the block layer start recovery for the
 * request by deleting the timer and calling the q's timeout function.
 * LLDDs who implement their own error recovery MAY ignore the timeout
 * event if they generated blk_abort_req. Must hold queue lock.
 */
void blk_abort_request(struct request *req)
{
	if (blk_mark_rq_complete(req))
		return;
	blk_delete_timer(req);
	blk_rq_timed_out(req);
}
EXPORT_SYMBOL_GPL(blk_abort_request);

/**
 * blk_add_timer - Start timeout timer for a single request
 * @req:	request that is about to start running.
 *
 * Notes:
 *    Each request has its own timer, and as it is added to the queue, we
 *    set up the timer. When the request completes, we cancel the timer.
 */
void blk_add_timer(struct request *req)
{
	struct request_queue *q = req->q;
	unsigned long expiry;

	if (!q->rq_timed_out_fn)
		return;

	BUG_ON(!list_empty(&req->timeout_list));
	BUG_ON(test_bit(REQ_ATOM_COMPLETE, &req->atomic_flags));

	if (req->timeout)
		req->deadline = jiffies + req->timeout;
	else {
		req->deadline = jiffies + q->rq_timeout;
		/*
		 * Some LLDs, like scsi, peek at the timeout to prevent
		 * a command from being retried forever.
		 */
		req->timeout = q->rq_timeout;
	}
	list_add_tail(&req->timeout_list, &q->timeout_list);

	/*
	 * If the timer isn't already pending or this timeout is earlier
	 * than an existing one, modify the timer. Round up to next nearest
	 * second.
	 */
	expiry = round_jiffies_up(req->deadline);

	if (!timer_pending(&q->timeout) ||
	    time_before(expiry, q->timeout.expires))
		mod_timer(&q->timeout, expiry);
}

/**
 * blk_abort_queue -- Abort all request on given queue
 * @queue:	pointer to queue
 *
 */
void blk_abort_queue(struct request_queue *q)
{
	unsigned long flags;
	struct request *rq, *tmp;

	spin_lock_irqsave(q->queue_lock, flags);

	elv_abort_queue(q);

	list_for_each_entry_safe(rq, tmp, &q->timeout_list, timeout_list)
		blk_abort_request(rq);

	spin_unlock_irqrestore(q->queue_lock, flags);

}
EXPORT_SYMBOL_GPL(blk_abort_queue);

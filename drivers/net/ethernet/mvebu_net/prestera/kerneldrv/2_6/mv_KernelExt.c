#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/threads.h>

#ifdef  MVKERNELEXT_SYSCALLS
#  include <linux/syscalls.h>
#  include <asm/unistd.h>
#endif

#include "mv_KernelExt.h"

#ifdef ENABLE_REALTIME_SCHEDULING_POLICY
#  define MV_PRIO_MIN   1
#  define MV_PRIO_MAX   (MAX_USER_RT_PRIO-20)
#else
#  define MV_PRIO_MIN   MAX_RT_PRIO
#  define MV_PRIO_MAX   MAX_PRIO
#endif

#ifdef CONFIG_SMP
 
DEFINE_SPINLOCK(mv_giantlock);
#endif

static int                  mvKernelExt_major = MVKERNELEXT_MAJOR;
static int                  mvKernelExt_minor = MVKERNELEXT_MINOR;
static int                  mvKernelExt_initialized = 0;
static int                  mvKernelExt_opened = 0;
static struct cdev          mvKernelExt_cdev;

module_param(mvKernelExt_major, int, S_IRUGO);
module_param(mvKernelExt_minor, int, S_IRUGO);
MODULE_AUTHOR("Marvell Semi.");
MODULE_LICENSE("GPL");

static ssize_t mvKernelExt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    return -ERESTARTSYS;
}

static ssize_t mvKernelExt_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    return -ERESTARTSYS;
}

static loff_t mvKernelExt_lseek(struct file *filp, loff_t off, int whence)
{
    return -ERESTARTSYS;
}

#include "../common/mv_KernelExt.c"
#include "../common/mv_KernelExtSem.c"
#include "../common/mv_KernelExtMsgQ.c"

static void mv_waitqueue_init(
        mv_waitqueue_t* queue
)
{
    memset(queue, 0, sizeof(*queue));
}

static void mv_waitqueue_cleanup(
        mv_waitqueue_t* queue
)
{
    memset(queue, 0, sizeof(*queue));
}

static void mv_waitqueue_add(
        mv_waitqueue_t* queue,
        struct mv_task* tsk
)
{
    tsk->waitqueue = queue;
    tsk->wait_next = NULL;
    if (queue->first == NULL)
    {
        queue->first = tsk;
    } else {
        queue->last->wait_next = tsk;
    }
    queue->last = tsk;
}

static void mv_waitqueue_wake_first(
        mv_waitqueue_t* queue
)
{
    struct mv_task *p;

    p = queue->first;

    if (!p)
        return;

    if (p->tasklockflag == 1)
        p->task->state &= ~TASK_INTERRUPTIBLE;
    else if (p->tasklockflag == 2)
        wake_up_process(p->task);
    p->tasklockflag = 0;

    p->waitqueue = NULL;
    queue->first = p->wait_next;
    if (queue->first == NULL)
        queue->last = NULL;
}

static void mv_waitqueue_wake_all(
        mv_waitqueue_t* queue
)
{
    struct mv_task *p;

    p = queue->first;

    if (!p)
        return;

    while (p)
    {
        if (p->tasklockflag == 1)
            p->task->state &= ~TASK_INTERRUPTIBLE;
        else if (p->tasklockflag == 2)
            wake_up_process(p->task);
        p->tasklockflag = 0;

        p->waitqueue = NULL;
        p = p->wait_next;
    }

    queue->first = queue->last = NULL;
}

static void mv_delete_from_waitqueue(
        struct mv_task* tsk
)
{
    mv_waitqueue_t* queue;
    struct mv_task* p;

    queue = tsk->waitqueue;
    if (!queue)
        return;

    tsk->waitqueue = NULL;

    if (queue->first == tsk)
    {
        queue->first = tsk->wait_next;
        if (queue->first == NULL)
            queue->last = NULL;
        return;
    }

    for (p = queue->first; p; p = p->wait_next)
    {
        if (p->wait_next == tsk)
        {
            p->wait_next = tsk->wait_next;
            if (p->wait_next == NULL)
                queue->last = p;
            return;
        }
    }

    if (p->tasklockflag == 1)
        p->task->state &= ~TASK_INTERRUPTIBLE;
    else if (p->tasklockflag == 2)
        wake_up_process(p->task);
    p->tasklockflag = 0;

}

static int mv_do_short_wait_on_queue(
        mv_waitqueue_t* queue,
        struct mv_task* tsk,
        struct task_struct** owner
)
{
    mv_waitqueue_add(queue, tsk);

    tsk->tasklockflag = 1;
#ifndef ENABLE_REALTIME_SCHEDULING_POLICY
    while (tsk->waitqueue)
    {
        if (unlikely(signal_pending(tsk->task)))
        {
            tsk->tasklockflag = 0;
            mv_delete_from_waitqueue(tsk);
            return -1;
        }
        tsk->task->state |= TASK_INTERRUPTIBLE;
        MV_GLOBAL_UNLOCK();
        yield();
        MV_GLOBAL_LOCK();
    }
    return 0;
#else
    while (tsk->waitqueue)
    {
        if (rt_task(tsk->task) && *owner)
        {
            if (tsk->task->prio <= (*owner)->prio)
            {
                 
                break;
            }
        }
        if (unlikely(signal_pending(tsk->task)))
        {
            tsk->tasklockflag = 0;
            mv_delete_from_waitqueue(tsk);
            return -1;
        }
        tsk->task->state |= TASK_INTERRUPTIBLE;
        MV_GLOBAL_UNLOCK();
        yield();
        MV_GLOBAL_LOCK();
    }
    if (!tsk->waitqueue)
        return 0;

    tsk->tasklockflag = 2;
    while (tsk->waitqueue)
    {
        if (unlikely(signal_pending(tsk->task)))
        {
            tsk->tasklockflag = 0;
            mv_delete_from_waitqueue(tsk);
            return -1;
        }
        set_task_state(tsk->task, TASK_INTERRUPTIBLE);
        MV_GLOBAL_UNLOCK();
        schedule();
        MV_GLOBAL_LOCK();
    }
    return 0;
#endif
}

static int mv_do_wait_on_queue(
        mv_waitqueue_t* queue,
        struct mv_task* tsk
)
{
    int cnt = 200;

    mv_waitqueue_add(queue, tsk);

    tsk->tasklockflag = 1;
    while (tsk->waitqueue && cnt--)
    {
        if (unlikely(signal_pending(tsk->task)))
        {
            tsk->tasklockflag = 0;
            mv_delete_from_waitqueue(tsk);
            return -1;
        }
        tsk->task->state |= TASK_INTERRUPTIBLE;
        MV_GLOBAL_UNLOCK();
        yield();
        MV_GLOBAL_LOCK();
        if (!tsk->waitqueue)
            return 0;
    }

    tsk->tasklockflag = 2;
    while (tsk->waitqueue)
    {
        if (unlikely(signal_pending(tsk->task)))
        {
            tsk->tasklockflag = 0;
            mv_delete_from_waitqueue(tsk);
            return -1;
        }
        set_task_state(tsk->task, TASK_INTERRUPTIBLE);
        MV_GLOBAL_UNLOCK();
        schedule();
        MV_GLOBAL_LOCK();
    }
    return 0;
}

static unsigned long mv_do_wait_on_queue_timeout(
        mv_waitqueue_t* queue,
        struct mv_task* tsk,
        unsigned long timeout
)
{
    mv_waitqueue_add(queue, tsk);

    tsk->tasklockflag = 2;
    while (tsk->waitqueue && timeout)
    {
        if (unlikely(signal_pending(tsk->task)))
        {
            tsk->tasklockflag = 0;
            mv_delete_from_waitqueue(tsk);
            return -1;
        }
        set_task_state(tsk->task, TASK_INTERRUPTIBLE);
        MV_GLOBAL_UNLOCK();
        timeout = schedule_timeout(timeout);
        MV_GLOBAL_LOCK();
    }
    if (tsk->waitqueue)  
    {
        tsk->tasklockflag = 0;
        mv_delete_from_waitqueue(tsk);
    }
    return timeout;
}

static void mv_check_tasks(void)
{
    int k;

    MV_GLOBAL_LOCK();
    for (k = 0; k < mv_num_tasks; )
    {
         
        struct task_struct *p, *g;
        int found = 0;
        do_each_thread(g, p) {
            if (p == mv_tasks[k]->task)
                found = 1;
        } while_each_thread(g, p);
        if (!found)
        {
             
            mv_unregistertask(mv_tasks[k]->task);
            continue;
        }
        if (mv_tasks[k]->task->exit_state)
        {
            mv_unregistertask(mv_tasks[k]->task);
            continue;
        }
        k++;
    }
    MV_GLOBAL_UNLOCK();
}

static int translate_priority(
        int policy,
        int priority
)
{
    if (policy == SCHED_NORMAL)
        return 0;

    if (priority < 0 || priority > 255)
        return MV_PRIO_MAX;

#ifdef  CPU_ARM
    if (priority <= 10)
        return MV_PRIO_MAX-1;
#endif
     
    priority = 255 - priority;

    priority *= (MV_PRIO_MAX-MV_PRIO_MIN);
    priority >>= 8;
    priority += MV_PRIO_MIN;
    if (priority >= MV_PRIO_MAX)
        priority = MV_PRIO_MAX-1;

    return( priority );
}

static int mv_set_prio(mv_priority_stc *param)
{
    struct mv_task* p;
    struct sched_param prio;
    int policy;

    MV_GLOBAL_LOCK();
    p = get_task_by_id(param->taskid);
    MV_GLOBAL_UNLOCK();

    if (p == NULL)
        return -MVKERNELEXT_EINVAL;

    p->vxw_priority = param->vxw_priority;

#ifdef ENABLE_REALTIME_SCHEDULING_POLICY
    policy = SCHED_RR;
#else
    policy = SCHED_NORMAL;
#endif
    prio.sched_priority = translate_priority(policy, param->vxw_priority);
    sched_setscheduler(p->task, policy, &prio);

    return 0;
}

#if defined(MY_ABC_HERE) && defined(CONFIG_OF)
 
#else  
 
static int mvKernelExt_read_proc_mem(
        char    *page,
        char    **start,
        off_t   offset,
        int     count,
        int     *eof,
        void    *data
)
{
    int len;
    int k;
    int begin = 0;

    len = 0;
    len += sprintf(page+len,"mvKernelExt major=%d\n", mvKernelExt_major);
#ifdef MV_TASKLOCK_STAT
    len += sprintf(page+len,"tasklock count=%d wait=%d\n",
            mv_tasklock_lcount,
            mv_tasklock_wcount);
#endif

    len += sprintf(page+len,"lock owner=%d\n", mv_tasklock_owner?mv_tasklock_owner->pid:0);
    {
        struct mv_task* p;
        for (p = mv_tasklock_waitqueue.first; p; p = p->wait_next)
            len += sprintf(page+len," wq: %d\n", p->task->pid);
    }
     
    for (k = 0; k < mv_num_tasks; k++)
    {
        len += sprintf(page+len,
                "  task id=%d state=0x%x prio=%d(%d) flag=%d"
#ifdef  MV_TASKLOCK_STAT
                " tlcount=%d tlwait=%d"
#endif
                " name=\"%s\"\n",
                mv_tasks[k]->task->pid,
                (unsigned int)mv_tasks[k]->task->state,
                mv_tasks[k]->task->prio,
                mv_tasks[k]->vxw_priority,
                mv_tasks[k]->tasklockflag,
#ifdef  MV_TASKLOCK_STAT
                mv_tasks[k]->tasklock_lcount,
                mv_tasks[k]->tasklock_wcount,
#endif
                mv_tasks[k]->name);
        if (len+begin < offset)
        {
            begin += len;
            len = 0;
        }
        if (len+begin >= offset+count)
            break;
    }

    if (len+begin < offset)
        *eof = 1;
    offset -= begin;
    *start = page + offset;
    len -= offset;
    if (len > count)
        len = count;
    if (len < 0)
        len = 0;

    return len;
}
#endif  

void mvKernelExt_cleanup(void)
{
    printk("mvKernelExt Says: Bye world from kernel\n");

    mvKernelExt_cleanup_common();

    mvKernelExt_initialized = 0;

    remove_proc_entry("mvKernelExt", NULL);

    unregister_chrdev_region(MKDEV(mvKernelExt_major, mvKernelExt_minor), 1);

    cdev_del(&mvKernelExt_cdev);

}

int mvKernelExt_init(void)
{
    int         result = 0;

    printk(KERN_DEBUG "mvKernelExt_init\n");

    result = register_chrdev_region(
            MKDEV(mvKernelExt_major, mvKernelExt_minor),
            1, "mvKernelExt");

    if (result < 0)
    {
        printk("mvKernelExt_init: register_chrdev_region err= %d\n", result);
        return result;
    }

    cdev_init(&mvKernelExt_cdev, &mvKernelExt_fops);

    mvKernelExt_cdev.owner = THIS_MODULE;

    result = cdev_add(&mvKernelExt_cdev,
            MKDEV(mvKernelExt_major, mvKernelExt_minor), 1);
    if (result)
    {
        unregister_chrdev_region(MKDEV(mvKernelExt_major, mvKernelExt_minor), 1);
        printk("mvKernelExt_init: cdev_add err= %d\n", result);
        return result;
    }

    printk(KERN_DEBUG "mvKernelExt_major = %d cdev.dev=%x\n",
            mvKernelExt_major, mvKernelExt_cdev.dev);

    result = mvKernelExt_init_common();
    if (result)
    {
        unregister_chrdev_region(MKDEV(mvKernelExt_major, mvKernelExt_minor), 1);
        cdev_del(&mvKernelExt_cdev);
        return result;
    }

#ifndef CONFIG_OF
     
    create_proc_read_entry("mvKernelExt", 0, NULL, mvKernelExt_read_proc_mem, NULL);
#endif

    mvKernelExt_initialized = 1;

    printk(KERN_DEBUG "mvKernelExt_init finished\n");

    return 0;
}

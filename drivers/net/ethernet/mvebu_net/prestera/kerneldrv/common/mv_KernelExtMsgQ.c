#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#ifdef CONFIG_OF
#include <linux/proc_fs.h>
#endif

#define MV_MSGQ_STAT

typedef struct _mvMsgQ
{
    int                     flags;
    char                    name[MV_MSGQ_NAME_LEN+1];
    mv_waitqueue_t          rxWaitQueue;
    mv_waitqueue_t          txWaitQueue;
    int                     maxMsgs;
    int                     maxMsgSize;
    int                     messages;
    char                    *buffer;
    int                     head;
    int                     tail;
    int                     waitRx;
    int                     waitTx;
} mvMsgQSTC;

static mvMsgQSTC        *mvMsgQs = NULL;
static int              mv_num_queues = MV_QUEUES_DEF;

module_param(mv_num_queues, int, S_IRUGO);

#if defined(MY_ABC_HERE) && defined(CONFIG_OF)
 
#else  
 
static int mvKernelExtMsgQ_read_proc_mem(
        char * page,
        char **start,
        off_t offset,
        int count,
        int *eof,
        void *data)
{
    int len;
    int k;
    int begin = 0;

    len = 0;

    len += sprintf(page+len,"id msgs waitRx waitTx");
    len += sprintf(page+len," name\n");
    for (k = 1; k < mv_num_queues; k++)
    {
        mvMsgQSTC *q;
        struct mv_task *p;

        if (!mvMsgQs[k].flags)
            continue;
        q = mvMsgQs + k;

        len += sprintf(page+len,"%d %d %d %d",
                k, q->messages, q->waitRx, q->waitTx);
        if (q->name[0])
            len += sprintf(page+len," %s", q->name);
        page[len++] = '\n';

        for (p = q->rxWaitQueue.first; p; p = p->wait_next)
            len += sprintf(page+len, "  rq=%d\n", p->task->pid);
        for (p = q->txWaitQueue.first; p; p = p->wait_next)
            len += sprintf(page+len, "  tq=%d\n", p->task->pid);

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

#ifdef CONFIG_OF
static int proc_status_show_msq(struct seq_file *m, void *v) {

#if defined(MY_ABC_HERE)
	 
#else  
    int len;
#endif  
    int k;

    seq_printf(m, "id msgs waitRx waitTx");
    seq_printf(m, " name\n");
    for (k = 1; k < mv_num_queues; k++)
    {
        mvMsgQSTC *q;
        struct mv_task *p;

        if (!mvMsgQs[k].flags)
            continue;
        q = mvMsgQs + k;

        seq_printf(m, "%d %d %d %d",
                k, q->messages, q->waitRx, q->waitTx);
        if (q->name[0])
            seq_printf(m, " %s", q->name);
	seq_putc(m, '\n');

        for (p = q->rxWaitQueue.first; p; p = p->wait_next)
            seq_printf(m, "  rq=%d\n", p->task->pid);
        for (p = q->txWaitQueue.first; p; p = p->wait_next)
            seq_printf(m, "  tq=%d\n", p->task->pid);

    }

    return 0;

}

static int proc_status_open_msq(struct inode *inode, struct file *file)
{
	return single_open(file, proc_status_show_msq, PDE_DATA(inode));
}

static const struct file_operations mv_kext_msq_read_proc_operations = {
	.open		= proc_status_open_msq,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif

static int mvKernelExt_MsgQInit(void)
{
    if (mv_num_queues < MV_QUEUES_MIN)
        mv_num_semaphores = MV_QUEUES_MIN;

    mvMsgQs = (mvMsgQSTC*) kmalloc(
            mv_num_queues * sizeof(mvMsgQSTC), GFP_KERNEL);

    if (mvMsgQs == NULL)
    {
        return 0;
    }

    memset(mvMsgQs, 0, mv_num_queues * sizeof(mvMsgQSTC));

#ifdef CONFIG_OF
	if (!proc_create("mvKernelExtMsgQ", S_IRUGO, NULL, &mv_kext_msq_read_proc_operations))
		return -ENOMEM;
#else
    create_proc_read_entry("mvKernelExtMsgQ", 0, NULL, mvKernelExtMsgQ_read_proc_mem, NULL);
#endif

    return 1;
}

static void mvKernelExt_DeleteAllMsgQ(void)
{
    int k;

    for (k = 1; k < mv_num_queues; k++)
    {
        if (mvMsgQs[k].flags)
        {
            mv_waitqueue_wake_all(&(mvMsgQs[k].rxWaitQueue));
            mv_waitqueue_wake_all(&(mvMsgQs[k].txWaitQueue));
            mv_waitqueue_cleanup(&(mvMsgQs[k].rxWaitQueue));
            mv_waitqueue_cleanup(&(mvMsgQs[k].txWaitQueue));
        }
        mvMsgQs[k].flags = 0;
    }
}

static void mvKernelExt_MsgQCleanup(void)
{
    MV_GLOBAL_LOCK();

    if (mvMsgQs)
    {
        mvKernelExt_DeleteAllMsgQ();
        kfree(mvMsgQs);
    }

    mvMsgQs = NULL;

    MV_GLOBAL_UNLOCK();

    remove_proc_entry("mvKernelExtMsgQ", NULL);
}

int mvKernelExt_MsgQCreate(
    const char *name,
    int maxMsgs,
    int maxMsgSize
)
{
    int k;
    mvMsgQSTC *q = NULL;

    MV_GLOBAL_LOCK();

    for (k = 1; k < mv_num_queues; k++)
    {
        if (mvMsgQs[k].flags == 0)
            break;
    }
    if (k >= mv_num_queues)
    {
        MV_GLOBAL_UNLOCK();
        return -MVKERNELEXT_ENOMEM;
    }
    q = mvMsgQs + k;

    memset(q, 0, sizeof(*q));
    q->flags = 3;
    MV_GLOBAL_UNLOCK();

    maxMsgSize = (maxMsgSize+3) & ~3;
    q->maxMsgs = maxMsgs;
    q->maxMsgSize = maxMsgSize;
    q->buffer = (char*)kmalloc((maxMsgSize + sizeof(int))*maxMsgs, GFP_KERNEL);
    if (q->buffer == NULL)
    {
        q->flags = 0;
        return -MVKERNELEXT_ENOMEM;
    }

    MV_GLOBAL_LOCK();
    q->flags = 1;
    mv_waitqueue_init(&(q->rxWaitQueue));
    mv_waitqueue_init(&(q->txWaitQueue));

    strncpy(q->name, name, MV_MSGQ_NAME_LEN);
    q->name[MV_MSGQ_NAME_LEN] = 0;

    MV_GLOBAL_UNLOCK();
    return k;
}

#define MSGQ_BY_ID(msgId) \
    MV_GLOBAL_LOCK(); \
    if (unlikely(msgqId == 0 || msgqId >= mv_num_queues)) \
    { \
ret_einval: \
        MV_GLOBAL_UNLOCK(); \
        return -MVKERNELEXT_EINVAL; \
    } \
    q = mvMsgQs + msgqId; \
    if (unlikely(q->flags != 1)) \
        goto ret_einval;

#define CHECK_MSGQ() \
    if (unlikely(q->flags != 1)) \
    { \
        MV_GLOBAL_UNLOCK(); \
        return -MVKERNELEXT_EDELETED; \
    }

int mvKernelExt_MsgQDelete(int msgqId)
{
    mvMsgQSTC *q;
    int timeOut;

    MSGQ_BY_ID(msgqId);

    q->flags = 2;  

    for (timeOut = HZ; q->waitRx && timeOut; timeOut--)
    {
        mv_waitqueue_wake_all(&(q->rxWaitQueue));
        if (q->waitRx)
        {
            MV_GLOBAL_UNLOCK();
            schedule_timeout(1);
            MV_GLOBAL_LOCK();
        }
    }
    for (timeOut = HZ; q->waitTx && timeOut; timeOut--)
    {
        mv_waitqueue_wake_all(&(q->txWaitQueue));
        if (q->waitTx)
        {
            MV_GLOBAL_UNLOCK();
            schedule_timeout(1);
            MV_GLOBAL_LOCK();
        }
    }

    mv_waitqueue_cleanup(&(q->rxWaitQueue));
    mv_waitqueue_cleanup(&(q->txWaitQueue));

    MV_GLOBAL_UNLOCK();
    kfree(q->buffer);

    q->flags = 0;

    return 0;
}

int mvKernelExt_MsgQSend(
    int     msgqId,
    void*   message,
    int     messageSize,
    int     timeOut,
    int     userspace
)
{
    char    *msg;
    mvMsgQSTC *q;

    MSGQ_BY_ID(msgqId);

    while (q->messages == q->maxMsgs)
    {
         
        if (timeOut == 0)
        {
            MV_GLOBAL_UNLOCK();
            return -MVKERNELEXT_EFULL;  
        }
        else
        {
            TASK_WILL_WAIT(current);
            q->waitTx++;
            if (timeOut != -1)
            {
#if HZ != 1000
                timeOut += 1000 / HZ - 1;
                timeOut /= 1000 / HZ;
#endif
                timeOut = mv_do_wait_on_queue_timeout(&(q->txWaitQueue), p, timeOut);
                CHECK_MSGQ();
                if (timeOut == 0)
                {
                    q->waitTx--;
                    MV_GLOBAL_UNLOCK();
                    return -MVKERNELEXT_ETIMEOUT;
                }
                if (timeOut == (unsigned long)(-1))
                {
                    q->waitTx--;
                    MV_GLOBAL_UNLOCK();
                    return -MVKERNELEXT_EINTR;
                }
            }
            else  
            {
                if (unlikely(mv_do_wait_on_queue(&(q->txWaitQueue), p)))
                {
                    q->waitTx--;
                    MV_GLOBAL_UNLOCK();
                    return -MVKERNELEXT_EINTR;
                }
                CHECK_MSGQ();
            }
            q->waitTx--;
        }
    }

    msg = q->buffer + q->head * (q->maxMsgSize + sizeof(int));
    if (messageSize > q->maxMsgSize)
        messageSize = q->maxMsgSize;

    *((int*)msg) = messageSize;
    if (userspace)
    {
        if (copy_from_user(msg+sizeof(int), message, messageSize))
        {
            MV_GLOBAL_UNLOCK();
            return -MVKERNELEXT_EINVAL;
        }
    }
    else
    {
        memcpy(msg+sizeof(int), message, messageSize);

    }
    q->head++;
    if (q->head >= q->maxMsgs)  
        q->head = 0;
    q->messages++;

    if (q->waitRx)
    {
        mv_waitqueue_wake_first(&(q->rxWaitQueue));
         
    }

    MV_GLOBAL_UNLOCK();
    return 0;
}

int mvKernelExt_MsgQRecv(
    int     msgqId,
    void*   message,
    int     messageSize,
    int     timeOut,
    int     userspace
)
{
    char    *msg;
    int  msgSize;
    mvMsgQSTC *q;

    MSGQ_BY_ID(msgqId);

    while (q->messages == 0)
    {
         
        if (timeOut == 0)
        {
            MV_GLOBAL_UNLOCK();
            return -MVKERNELEXT_EEMPTY;  
        }
        else
        {
            TASK_WILL_WAIT(current);
            q->waitRx++;
            if (timeOut != -1)
            {
#if HZ != 1000
                timeOut += 1000 / HZ - 1;
                timeOut /= 1000 / HZ;
#endif
                timeOut = mv_do_wait_on_queue_timeout(&(q->rxWaitQueue), p, timeOut);
                CHECK_MSGQ();
                if (timeOut == 0)
                {
                    q->waitRx--;
                    MV_GLOBAL_UNLOCK();
                    return -MVKERNELEXT_ETIMEOUT;
                }
                if (timeOut == (unsigned long)(-1))
                {
                    q->waitRx--;
                    MV_GLOBAL_UNLOCK();
                    return -MVKERNELEXT_EINTR;
                }
            }
            else  
            {
                if (unlikely(mv_do_wait_on_queue(&(q->rxWaitQueue), p)))
                {
                    q->waitRx--;
                    MV_GLOBAL_UNLOCK();
                    return -MVKERNELEXT_EINTR;
                }
                CHECK_MSGQ();
            }
            q->waitRx--;
        }
    }
     
    msg = q->buffer + q->tail * (q->maxMsgSize + sizeof(int));
    msgSize = *((int*)msg);
    if (msgSize > messageSize)
        msgSize = messageSize;

    if (userspace)
    {
        if (copy_to_user(message, msg+sizeof(int), msgSize))
        {
            msgSize = 0;
        }
    }
    else
    {
        memcpy(message, msg+sizeof(int), msgSize);
    }
    q->tail++;
    if (q->tail >= q->maxMsgs)  
        q->tail = 0;
    q->messages--;

    if (q->waitTx)
    {
        mv_waitqueue_wake_first(&(q->txWaitQueue));
         
    }

    MV_GLOBAL_UNLOCK();
    return msgSize;
}

int mvKernelExt_MsgQNumMsgs(int msgqId)
{
    int numMessages;
    mvMsgQSTC *q;

    MSGQ_BY_ID(msgqId);
    numMessages = q->messages;
    MV_GLOBAL_UNLOCK();

    return numMessages;
}

EXPORT_SYMBOL(mvKernelExt_MsgQCreate);
EXPORT_SYMBOL(mvKernelExt_MsgQDelete);
EXPORT_SYMBOL(mvKernelExt_MsgQSend);
EXPORT_SYMBOL(mvKernelExt_MsgQRecv);
EXPORT_SYMBOL(mvKernelExt_MsgQNumMsgs);

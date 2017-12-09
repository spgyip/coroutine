#ifndef __CRT_SCHED_H__
#define __CRT_SCHED_H__

#include <time.h>
#include "rbtree.h"
#include "list.h"
#include "memcache.h"

#include "coro_switch.h"

struct timer_node
{
    struct rb_node node;    //用于连接inactive
    struct rb_root root;    //用于作为相同超时时间的协程的根
    time_t timeout;         //超时时间
};

typedef void (*coro_func)(void *args);

struct coroutine
{
    struct list_head list;      //挂在coro_schedule中的free_pool或者active链表上
    struct rb_node node;        //协程休眠时, 挂在timer_node的root上,
                                //提高网络事件来临时的查找效率
    unsigned long long coro_id; //协程号
    struct context ctx;         //本协程栈顶指针描述
    struct coro_stack stack;    //协程栈描述
    coro_func func;             //具体执行函数
    void *args;                 //执行函数参数

    time_t timeout;             //未来某个时间, 用于跟踪网络事件超时
    int active_by_timeout;      //是事件到来唤醒的(0), 还是超时唤醒的(1)
};

/* sched policy, if timeout_milliseconds == -1 sched policy can wait forever */
typedef void (*sched_policy)(int timeout_seconds);

struct coro_schedule
{
    size_t min_coro_size;       //协程最少个数
    size_t max_coro_size;       //协程最大个数
    size_t curr_coro_size;      //当前协程数

    unsigned long long next_coro_id;    //下一个协程号

    size_t stack_bytes;         //协程的栈大小, 字节单位
    struct coroutine main_coro; //主协程
    struct coroutine *current;  //当前协程

    struct list_head idle;      //空闲的协程列表
    struct list_head active;    //活跃的协程列表
    struct rb_root inactive;    //处于等待中的协程
    struct memcache *cache;     //timer_tree的cache

    sched_policy policy;        //调度策略
};

void scheduler();
int gen_worker(coro_func func, void *args);
void yield();
void schedule_timeout(int seconds);
void schedule_create(size_t stack_kbytes, size_t min_coro_size, size_t max_coro_size);

void test();

#endif


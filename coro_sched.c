#include <stdio.h>  //del
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "coro_sched.h"

static __thread struct coro_schedule g_sched;

#include <unistd.h> //del
static void printf_active_list()
{
    struct coroutine *coro;

    printf("printf_active_list:\n");
    list_for_each_entry(coro, &g_sched.active, list)
    {
        printf("\tcoro:%p id:%llu\n", coro, coro->coro_id);
    }
}

static void printf_free_list()
{
    struct coroutine *coro;

    printf("printf_free_list:\n");
    list_for_each_entry(coro, &g_sched.idle, list)
    {
        printf("\tcoro:%p id:%llu\n", coro, coro->coro_id);
    }
}

static void printf_list()
{
    printf_active_list();
    printf_free_list();
    printf("\n\n");
}

#if 1

static void printf_timer_node(struct rb_node *each)
{
    if (each)
    {
        printf_timer_node(each->rb_left);

        struct coroutine *coro = container_of(each, struct coroutine, node);
        printf("\ttimeout:%lu coro id:%llu\n", coro->timeout, coro->coro_id);

        printf_timer_node(each->rb_right);
    }
}

static void __printf_inactive_tree(struct rb_node *each)
{
    if (each)
    {
        __printf_inactive_tree(each->rb_left);

        struct timer_node *node = container_of(each, struct timer_node, node);
        printf("timeout:%lu, coro:\n", node->timeout);
        printf_timer_node(node->root.rb_node);

        __printf_inactive_tree(each->rb_right);
    }
}
static void printf_inactive_tree()
{
    __printf_inactive_tree(g_sched.inactive.rb_node);
}
#endif

static struct coroutine *find_in_timer(struct timer_node *tm_node, unsigned long long coro_id)
{
    struct rb_node *each = tm_node->root.rb_node;

    while (each)
    {
        struct coroutine *coro = container_of(each, struct coroutine, node);

        long long result = coro_id - coro->coro_id;
        if (result < 0)
            each = each->rb_left;
        else if (result > 0)
            each = each->rb_right;
        else
            return coro;
    }

    return NULL;
}

static void remove_from_timer_node(struct timer_node *tm_node,
                                   struct coroutine *coro)
{
    if (NULL == find_in_timer(tm_node, coro->coro_id))
        return;

    rb_erase(&coro->node, &tm_node->root);
}

static struct timer_node *find_timer_node(time_t time)
{
    struct rb_node *each = g_sched.inactive.rb_node;

    while (each)
    {
        struct timer_node *tm_node = container_of(each, struct timer_node, node);

        long long result = time - tm_node->timeout;
        if (result < 0)
            each = each->rb_left;
        else if (result > 0)
            each = each->rb_right;
        else
            return tm_node;
    }

    return NULL;
}

static inline void remove_from_inactive_tree(struct coroutine *coro)
{
    struct timer_node *tm_node = find_timer_node(coro->timeout);
    if (NULL == tm_node)
        return;

    remove_from_timer_node(tm_node, coro);

    if (RB_EMPTY_ROOT(&tm_node->root))
    {
        rb_erase(&tm_node->node, &g_sched.inactive);
        memcache_free(g_sched.cache, tm_node);
    }
}

static void add_to_timer_node(struct timer_node *tm_node, struct coroutine *coro)
{
    struct rb_node **new = &tm_node->root.rb_node, *parent = NULL;

    list_del_init(&coro->list);

    while (*new)
    {
        struct coroutine *each = container_of(*new, struct coroutine, node);
        long long result = coro->coro_id - each->coro_id;

        parent = *new;
        if (result < 0)
            new = &(*new)->rb_left;
        else if (result > 0)
            new = &(*new)->rb_right;
        else
        {
            printf("same coro id %llu\n", coro->coro_id);
            exit(-1);
        }
    }

    rb_link_node(&coro->node, parent, new);
    rb_insert_color(&coro->node, &tm_node->root);
}

static void move_to_inactive_tree(struct coroutine *coro)
{
    int timespan;
    struct timer_node *tm_node;
    struct rb_node **new = &g_sched.inactive.rb_node;
    struct rb_node *parent = NULL;

    while (*new)
    {
        tm_node = container_of(*new, struct timer_node, node);
        timespan = coro->timeout - tm_node->timeout;

        parent = *new;
        if (timespan < 0)
            new = &(*new)->rb_left;
        else if (timespan > 0)
            new = &(*new)->rb_right;
        else
        {
            add_to_timer_node(tm_node, coro);
            return;
        }
    }

    struct timer_node *tmp = memcache_alloc(g_sched.cache);
    if (NULL == tmp)
    {
        printf("system not have enough mem to run\n");
        return;
    }

    tmp->timeout = coro->timeout;

    add_to_timer_node(tmp, coro);
    rb_link_node(&tmp->node, parent, new);
    rb_insert_color(&tmp->node, &g_sched.inactive);
}

static inline void move_to_idle_list(struct coroutine *coro)
{
    remove_from_inactive_tree(coro);
    list_add_tail(&coro->list, &g_sched.idle);
}

static inline void move_to_active_list(struct coroutine *coro)
{
    remove_from_inactive_tree(coro);
    list_add_tail(&coro->list, &g_sched.active);
}

static void init_coroutine(struct coroutine *coro)
{
    INIT_LIST_HEAD(&coro->list);
    RB_CLEAR_NODE(&coro->node);
    coro->coro_id = ++g_sched.next_coro_id;
    coro->timeout = 0;
    coro->active_by_timeout = -1;
}

static struct coroutine *create_coroutine()
{
    int ret;
    struct coroutine *coro;

    if (g_sched.curr_coro_size == g_sched.max_coro_size)
        return NULL;

    coro = (struct coroutine *)malloc(sizeof(struct coroutine));
    if (NULL == coro)
        return NULL;

    ret = coro_stack_alloc(&coro->stack, g_sched.stack_bytes);
    if (ret)
    {
        free(coro);
        return NULL;
    }

    g_sched.curr_coro_size++;

    return coro;
}

#if 0
static void free_coroutine(struct coroutine *coro)
{
    list_del(&coro->list);
    remove_from_inactive_tree(coro);
    coro_stack_free(&coro->stack);
    free(coro);

    g_sched.curr_coro_size--;
}
#endif

static struct coroutine *get_coroutine()
{
    struct coroutine *coro;

    if (!list_empty(&g_sched.idle))
    {
        coro = list_first_entry(&g_sched.idle, struct coroutine, list);
        list_del(&coro->list);
        init_coroutine(coro);

        return coro;
    }

    coro = create_coroutine();
    if (NULL != coro)
        init_coroutine(coro);

    return coro;
}

static void coroutine_switch(struct coroutine *curr, struct coroutine *next)
{
    g_sched.current = next;
    context_switch(&curr->ctx, &next->ctx);
}

static void run_active_coroutine()
{
    //printf("start run_active_coroutine\n");
    //printf_list();

    struct coroutine *coro, *tmp;

    list_for_each_entry_safe(coro, tmp, &g_sched.active, list)
    {
        list_del(&coro->list);
        g_sched.current = coro;

        printf("switch to %llu\n", coro->coro_id);
        coroutine_switch(&g_sched.main_coro, coro);
    }

    //printf("end run_active_coroutine\n");
    //printf_list();
}

static struct timer_node *get_recent_timer_node()
{
    struct rb_node *recent = rb_first(&g_sched.inactive);
    if (NULL == recent)
        return NULL;

    return container_of(recent, struct timer_node, node);
}

static void handle_timeout_coroutine(struct timer_node *node)
{
    struct rb_node *recent;

    while (NULL != (recent = rb_first(&node->root)))
    {
        struct coroutine *coro = container_of(recent, struct coroutine, node);
        rb_erase(recent, &node->root);
        coro->active_by_timeout = 1;
        move_to_active_list(coro);
    }

    memcache_free(g_sched.cache, node);
}

static void check_timeout_coroutine()
{
    struct timer_node *node;
    time_t now = time(NULL);

    while (1)
    {
        node = get_recent_timer_node();
        if (NULL == node)
            return;

        if (now < node->timeout)
            return;

        rb_erase(&node->node, &g_sched.inactive);
        handle_timeout_coroutine(node);
    }
}

static int get_recent_timespan()
{
    int timespan;
    time_t now = time(NULL);
    struct timer_node *node = get_recent_timer_node();
    if (NULL == node)
        return 10;  //at least 10 seconds

    timespan = node->timeout - now;

    return (timespan < 0)? 0 : timespan;
}

void scheduler()
{
    while (1)
    {
        check_timeout_coroutine();
        run_active_coroutine();

        if (g_sched.policy)
        {
            int timeout_seconds = get_recent_timespan();
            g_sched.policy(timeout_seconds);
            run_active_coroutine();
        }
    }
}

static void coro_routine_proxy(void *args)
{
    struct coroutine *coro = args;

    coro->func(coro->args);

    printf("end exec, add to idle. %p\n", coro);
    move_to_idle_list(coro);
    coroutine_switch(g_sched.current, &g_sched.main_coro);
}

int gen_worker(coro_func func, void *args)
{
    struct coroutine *coro = get_coroutine();
    if (NULL == coro)
        return -1;

    coro->func = func;
    coro->args = args;

    coro_init(&coro->ctx, &coro->stack, coro_routine_proxy, coro);
    move_to_active_list(coro);

    return 0;
}

void yield()
{
    move_to_active_list(g_sched.current);
    coroutine_switch(g_sched.current, &g_sched.main_coro);
}

void schedule_timeout(int seconds)
{
    struct coroutine *coro = g_sched.current;
    coro->timeout = time(NULL) + seconds;
    move_to_inactive_tree(coro);
    coroutine_switch(g_sched.current, &g_sched.main_coro);
}

void test()
{
    int i;
    struct coroutine *coro;

    for (i = 1; i <= 100; i++)
    {
        coro = get_coroutine();
        coro->timeout = i;
        move_to_inactive_tree(coro);
    }

    printf_inactive_tree();
    move_to_active_list(coro);
    printf_inactive_tree();
    printf_list();
}

/*
    @stack_bytes:   协程栈大小, KB单位. 推荐4, 8, 16, 32, 64
    @min_coro_size: 协程最小个数, 推荐最小为1
    @max_coro_size: 协程最大个数, 推荐为网络服务的最大连接数
*/
void schedule_create(size_t stack_kbytes, size_t min_coro_size, size_t max_coro_size)
{
    int i;

    assert(min_coro_size <= max_coro_size && min_coro_size >= 1);

    g_sched.min_coro_size = min_coro_size;
    g_sched.max_coro_size = max_coro_size;
    g_sched.curr_coro_size = 0;
    g_sched.next_coro_id = 0;

    g_sched.stack_bytes = stack_kbytes * 1024;
    g_sched.current = NULL;

    INIT_LIST_HEAD(&g_sched.idle);
    INIT_LIST_HEAD(&g_sched.active);
    g_sched.inactive = RB_ROOT;
    g_sched.cache = memcache_create(max_coro_size / 2, sizeof(struct timer_node));
    g_sched.policy = NULL;
    if (NULL == g_sched.cache)
    {
        printf("Failed to create mem cache\n");
        exit(0);
    }

    for (i = 0; i < min_coro_size; i++)
    {
        struct coroutine *coro = create_coroutine();
        if (NULL == coro)
        {
            printf("Failed to create schedule system\n");
            exit(0);
        }

        move_to_idle_list(coro);
    }
}


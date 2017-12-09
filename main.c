#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if 1
#include "coro_sched.h"
#include <sys/resource.h>

static void sys_rlimit()
{
    struct rlimit rlim, rlim_new;

    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
    {
        rlim_new.rlim_cur = rlim_new.rlim_max = 100000;
        if (setrlimit(RLIMIT_NOFILE, &rlim_new) != 0)
        {
            printf("Failed to set rlimit file. exit!\n");
            exit(0);
        }
    }

    if (getrlimit(RLIMIT_CORE, &rlim) == 0)
    {
        rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &rlim_new) != 0)
        {
            printf("Failed to set rlimit core. exit!\n");
            exit(0);
        }
    }
}


void echo_1(void *args)
{
    printf("i am at echo 1. time :%lu\n", time(NULL));
    schedule_timeout(3);
    printf("i am back at echo 1. time :%lu\n", time(NULL));
}

void echo_2(void *args)
{
    printf("i am at echo 2. time :%lu\n", time(NULL));
    schedule_timeout(3);
    printf("i am back at echo 2. time :%lu\n", time(NULL));
}

void master(void *args)
{
    int *i = args;
    printf("want to sleep %d. time :%lu\n", *i, time(NULL));
    schedule_timeout(*i);
    printf("wake up %d. time :%lu\n", *i, time(NULL));
}

void maintest()
{
    #define size 100
    static int arr[size];
    int i;

    for (i = 1; i <= 100; i++)
    {
        arr[i - 1] = i;
        gen_worker(master, &arr[i - 1]);
    }

}

int main()
{
    sys_rlimit();
    schedule_create(32, 3, 200);
    maintest();
    scheduler();
    return 0;
}
#endif

#if 0
#include "min_heap.h"

int compare(void *s, void *t)
{
    int *is = s;
    int *it = t;

    if (*is > *it)
        return 1;

    if (*is == *it)
        return 0;

    return -1;
}

static void print_heap(struct min_heap *heap)
{
    int i = 0;
    for (; i < heap->curr; i++)
        printf("%d ", *((int *)(heap->array[i])));

    printf("\n");
}

int main()
{
    struct min_heap heap;

    int ret = min_heap_init(&heap, compare);
    if (ret)
    {
        printf("failed to init heap\n");
        exit(0);
    }

    for (int i = 10; i >= 0; i--)
    {
        int *k = (int *)malloc(sizeof(int));
        *k = i;

        min_heap_push(&heap, k);
        printf("size:%d\t", heap.size);
        print_heap(&heap);
    }

    for (int i = 11; i >= 0; i--)
    {
        int *k = min_heap_pop(&heap);
        if (NULL != k)
        {
            printf("pop:%d\t", *k);
            print_heap(&heap);
        }
        else
            printf("no elem\n");
    }


    return 0;
}
#endif

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "rbtree.h"

struct mynode
{
    struct rb_node node;
    int string;
};

struct rb_root mytree = RB_ROOT;

struct mynode *my_search(struct rb_root *root, int string)
{
    struct rb_node *node = root->rb_node;

    while (node)
    {
        struct mynode *data = container_of(node, struct mynode, node);

        int result = string - data->string;
        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return data;
    }

    return NULL;
}

int my_insert(struct rb_root *root, struct mynode *data)
{
    struct rb_node **new = &root->rb_node, *parent = NULL;

    /* Figure out where to put new node */
    while (*new)
    {
        struct mynode *each = container_of(*new, struct mynode, node);
        int result = data->string - each->string;

        parent = *new;
        if (result < 0)
            new = &(*new)->rb_left;
        else if (result > 0)
            new = &(*new)->rb_right;
        else
            return 0;
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);

    return 1;
}
void my_free(struct mynode *node)
{
    if (node != NULL)
        free(node);
}

#define NUM_NODES 32

int main()
{
    struct mynode *mn[NUM_NODES];

    /* *insert */
    int i = 0;
    printf("insert node from 1 to NUM_NODES(32): \n");
    for (; i < NUM_NODES; i++)
    {
        mn[i] = (struct mynode *)malloc(sizeof(struct mynode));
        mn[i]->string = i;
        my_insert(&mytree, mn[i]);
    }

    /* *search */
    struct rb_node *node;
    printf("search all nodes: \n");
    for (node = rb_first(&mytree); node; node = rb_next(node))
        printf("key = %d\n", rb_entry(node, struct mynode, node)->string);

    /* *delete */
    printf("delete node 20: \n");
    struct mynode *data = my_search(&mytree, 20);
    if (data)
    {
        rb_erase(&data->node, &mytree);
        my_free(data);
    }

    /* *delete again*/
    printf("delete node 10: \n");
    data = my_search(&mytree, 10);
    if (data)
    {
        rb_erase(&data->node, &mytree);
        my_free(data);
    }

    /* *delete once again*/
    printf("delete node 15: \n");
    data = my_search(&mytree, 15);
    if (data)
    {
        rb_erase(&data->node, &mytree);
        my_free(data);
    }

    /* *search again*/
    printf("search again:\n");
    for (node = rb_first(&mytree); node; node = rb_next(node))
        printf("key = %d\n", rb_entry(node, struct mynode, node)->string);

    return 0;
}
#endif

#if 0

#include "memcache.h"

int main()
{
    struct memcache *cache = memcache_create(10, sizeof(int));

    int loop = 1;
    while (loop++ <= 20)
    {
        void *i = memcache_alloc(cache);
    }

    loop = 1;
    while (loop++ <= 20)
    {
        void *i;
        memcache_free(cache, i);
    }

    /*

        void *i = memcache_alloc(cache);
        void *j = memcache_alloc(cache);
        memcache_free(cache, i);
        memcache_free(cache, j);

        i = memcache_alloc(cache);
        j = memcache_alloc(cache);
        void *k = memcache_alloc(cache);
        memcache_free(cache, i);
        memcache_free(cache, j);
        memcache_free(cache, k);

    */
    return 0;
}

#endif


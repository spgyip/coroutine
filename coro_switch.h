#ifndef __CRT_SWITCH_H__
#define __CRT_SWITCH_H__

struct coro_stack
{
    void *ptr;
    size_t size_bytes;
};

struct context
{
    void **sp;  //指向当前协程的栈顶, 也就是%esp/%rsp
};

typedef void (*coro_routine)(void *args);

void context_switch(struct context *prev, struct context *next)
                    __attribute__((__noinline__, __regparm__(2)));

int coro_stack_alloc(struct coro_stack *stack, size_t size_bytes);
void coro_stack_free(struct coro_stack *stack);

void coro_init(struct context *ctx, struct coro_stack *stack, coro_routine routine, void *args);


#endif


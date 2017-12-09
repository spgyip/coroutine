#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>

#include "coro_switch.h"

#define PAGE_SIZE   4096

/*
    如下4个线程安全局部变量仅仅在初始化一个协程时用到
 */
static __thread coro_routine g_func;
static void __thread *g_args;

static __thread struct context g_create_ctx;
static __thread struct context *g_new_ctx;

__asm__(
    "\t.text\n"
    "\t.globl context_switch\n"
    "context_switch:\n"
#if __amd64
#define NUM_SAVED 6
    "\tpushq %rbp\n"
    "\tpushq %rbx\n"
    "\tpushq %r12\n"
    "\tpushq %r13\n"
    "\tpushq %r14\n"
    "\tpushq %r15\n"
    "\tmovq %rsp, (%rdi)\n"
    "\tmovq (%rsi), %rsp\n"
    "\tpopq %r15\n"
    "\tpopq %r14\n"
    "\tpopq %r13\n"
    "\tpopq %r12\n"
    "\tpopq %rbx\n"
    "\tpopq %rbp\n"
    "\tpopq %rcx\n"
    "\tjmpq *%rcx\n"
#elif __i386
#define NUM_SAVED 4
    "\tpushl %ebp\n"
    "\tpushl %ebx\n"
    "\tpushl %esi\n"
    "\tpushl %edi\n"
    "\tmovl %esp, (%eax)\n"
    "\tmovl (%edx), %esp\n"
    "\tpopl %edi\n"
    "\tpopl %esi\n"
    "\tpopl %ebx\n"
    "\tpopl %ebp\n"
    "\tpopl %ecx\n"
    "\tjmpl *%ecx\n"
#else
#error unsupported architecture
#endif
);

static void coro_init_routine()
{
    coro_routine routine = g_func;
    void *args = g_args;

    context_switch(g_new_ctx, &g_create_ctx);

    routine(args);

    abort();
}

/*
    size大小会被自动转为4K对齐, 最小为4K
    建议值: 4K(4096), 32K(32*1024), 64K(64*1024)
 */
int coro_stack_alloc(struct coro_stack *stack, size_t size_bytes)
{
    if (size_bytes < PAGE_SIZE)
        size_bytes = PAGE_SIZE;

    size_bytes = (size_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    size_bytes *= PAGE_SIZE;

    stack->size_bytes = size_bytes;

    size_bytes += PAGE_SIZE;  //多分配一个页作为内存保护, 防止栈溢出而不知

    stack->ptr = mmap(0, size_bytes, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack->ptr == (void *)-1)
        return -1;

    mprotect(stack->ptr, PAGE_SIZE, PROT_NONE);
    stack->ptr = (void *)((char *)stack->ptr + size_bytes);

    return 0;
}

void coro_stack_free(struct coro_stack *stack)
{
    stack->ptr = (void *)((char *)stack->ptr - stack->size_bytes - PAGE_SIZE);
    munmap(stack->ptr, stack->size_bytes + PAGE_SIZE);
}

void coro_init(struct context *ctx, struct coro_stack *stack, coro_routine routine, void *args)
{
    assert(NULL != routine);

    ctx->sp = (void **)stack->ptr;

    *--ctx->sp = (void *)abort;
    *--ctx->sp = (void *)coro_init_routine;

    ctx->sp -= NUM_SAVED;

    memset(ctx->sp, 0, sizeof(*ctx->sp) * NUM_SAVED);

    g_func = routine;
    g_args = args;
    g_new_ctx = ctx;

    context_switch(&g_create_ctx, g_new_ctx);
}


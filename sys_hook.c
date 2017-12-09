#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define _GNU_SOURCE
#include <dlfcn.h>

typedef int (*sys_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*sys_accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
typedef ssize_t (*sys_read)(int fd, void *buf, size_t count);
typedef ssize_t (*sys_recv)(int sockfd, void *buf, size_t len, int flags);
typedef ssize_t (*sys_recvfrom)(int sockfd, void *buf, size_t len, int flags,
                                struct sockaddr *src_addr, socklen_t *addrlen);
typedef ssize_t (*sys_write)(int fd, const void *buf, size_t count);
typedef ssize_t (*sys_send)(int sockfd, const void *buf, size_t len, int flags);
typedef ssize_t (*sys_sendto)(int sockfd, const void *buf, size_t len, int flags,
                              const struct sockaddr *dest_addr, socklen_t addrlen);

static sys_connect g_sys_connect = NULL;
static sys_accept g_sys_accept = NULL;

static sys_read g_sys_read = NULL;
static sys_recv g_sys_recv = NULL;
static sys_recvfrom g_sys_recvfrom = NULL;

static sys_write g_sys_write = NULL;
static sys_send g_sys_send = NULL;
static sys_sendto g_sys_sendto = NULL;

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret;
    int flags;
    socklen_t len;

    set_nonblock(sockfd);

    ret = g_sys_connect(sockfd, addr, addrlen);

    if (ret != -1 || errno != EINPROGRESS)
        return ret;

    for(;;)
    {

        if (tcp_mtask_yield(sockfd, NGX_WRITE_EVENT))
        {
            errno = ETIMEDOUT;
            return -1;
        }

        len = sizeof(flags);

        flags = 0;

        ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &flags, &len);

        if (ret == -1 || !len)
            return -1;

        if (!flags)
            return 0;

        if (flags != EINPROGRESS)
        {
            errno = flags;
            return -1;
        }
    }
}

__attribute__((constructor)) static void init_syscall_hook()
{
#define HOOK_SYSCALL(name) g_sys_##name = (sys##name)dlsym(RTLD_NEXT, #name)

    HOOK_SYSCALL(connect);
    HOOK_SYSCALL(accept);

    HOOK_SYSCALL(read);
    HOOK_SYSCALL(recv);
    HOOK_SYSCALL(recvfrom);

    HOOK_SYSCALL(write);
    HOOK_SYSCALL(send);
    HOOK_SYSCALL(sendto);

#undef HOOK_SYSCALL
}


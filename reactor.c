#include "reactor.h"

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

static inline
int set_fd_polling(int queue, int fd, int action, long milliseconds)
{
    struct epoll_event chevent;
    chevent.data.fd = fd;
    chevent.events = EPOLLOUT | EPOLLIN |
                     EPOLLET | EPOLLERR |
                     EPOLLRDHUP | EPOLLHUP;
    if (milliseconds) {
        struct itimerspec newtime;
        newtime.it_value.tv_sec = newtime.it_interval.tv_sec =
                                  milliseconds / 1000;
        newtime.it_value.tv_nsec = newtime.it_interval.tv_nsec =
                                  (milliseconds % 1000) * 1000000;
        timerfd_settime(fd, 0, &newtime, NULL);
    }
    return epoll_ctl(queue, action, fd, &chevent);
}

int reactor_add(struct Reactor *reactor, int fd)
{
    assert(reactor->private.reactor_fd);
    assert(reactor->maxfd >= fd);
    /*
     * the `on_close` callback was likely called already by the user,
     * before calling this, and a new handler was probably assigned
     * (or mapped) to the fd.
     */
    reactor->private.map[fd] = 1;
    return set_fd_polling(reactor->private.reactor_fd, fd,
                          EPOLL_CTL_ADD, 0);
}

int reactor_add_timer(struct Reactor *reactor, int fd, long milliseconds)
{
    assert(reactor->private.reactor_fd);
    assert(reactor->maxfd >= fd);
    reactor->private.map[fd] = 1;
    return set_fd_polling(reactor->private.reactor_fd, fd,
                          EPOLL_CTL_ADD, milliseconds);
}

int reactor_remove(struct Reactor *reactor, int fd)
{
    assert(reactor->private.reactor_fd);
    assert(reactor->maxfd >= fd);
    reactor->private.map[fd] = 0;
    return set_fd_polling(reactor->private.reactor_fd, fd,
                          EPOLL_CTL_DEL, 0);
}

void reactor_close(struct Reactor *reactor, int fd)
{
    assert(reactor->maxfd >= fd);
    static pthread_mutex_t locker = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&locker);
    if (reactor->private.map[fd]) {
        close(fd);
        reactor->private.map[fd] = 0;
        pthread_mutex_unlock(&locker);
        if (reactor->on_close)
            reactor->on_close(reactor, fd);
        set_fd_polling(reactor->private.reactor_fd, fd, EPOLL_CTL_DEL, 0);
        return;
    }
    pthread_mutex_unlock(&locker);
}

void reactor_reset_timer(int fd)
{
    char data[sizeof(void *)];
    if (read(fd, &data, sizeof(void *)) < 0)
        data[0] = 0;
}

int reactor_make_timer(void)
{
    return timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
}

#define _WAIT_FOR_EVENTS_ \
    epoll_wait(reactor->private.reactor_fd, \
               ((struct epoll_event *) reactor->private.events), \
               REACTOR_MAX_EVENTS, REACTOR_TICK)

#define _GETFD_(_ev_) \
    ((struct epoll_event *) reactor->private.events)[(_ev_)].data.fd
#define _EVENTERROR_(_ev_) \
    (((struct epoll_event *) reactor->private.events)[(_ev_)].events & \
     (~(EPOLLIN | EPOLLOUT)))
#define _EVENTREADY_(_ev_) \
    (((struct epoll_event *) reactor->private.events)[(_ev_)].events & \
     EPOLLOUT)
#define _EVENTDATA_(_ev_) \
    (((struct epoll_event *) reactor->private.events)[(_ev_)].events & \
     EPOLLIN)

static void reactor_destroy(struct Reactor *reactor)
{
    if (reactor->private.map)
        free(reactor->private.map);
    if (reactor->private.events)
        free(reactor->private.events);
    if (reactor->private.reactor_fd)
        close(reactor->private.reactor_fd);
    reactor->private.map = NULL;
    reactor->private.events = NULL;
    reactor->private.reactor_fd = 0;
}

int reactor_init(struct Reactor *reactor)
{
    if (reactor->maxfd <= 0) return -1;
    reactor->private.reactor_fd = epoll_create1(0);
    reactor->private.map = calloc(1, reactor->maxfd + 1);
    reactor->private.events = calloc(sizeof(struct epoll_event),
                                     REACTOR_MAX_EVENTS);
    if (!reactor->private.reactor_fd || !reactor->private.map ||
        !reactor->private.events) {
        reactor_destroy(reactor);
        return -1;
    }
    return 0;
}

void reactor_stop(struct Reactor *reactor)
{
    if (!reactor->private.map || !reactor->private.reactor_fd)
        return;
    for (int i = 0; i <= reactor->maxfd; i++) {
        if (reactor->private.map[i]) {
            if (reactor->on_shutdown)
                reactor->on_shutdown(reactor, i);
            reactor_close(reactor, i);
        }
    }
    reactor_destroy(reactor);
}

int reactor_review(struct Reactor *reactor)
{
    if (!reactor->private.reactor_fd) return -1;

    /* set the last tick */
    time(&reactor->last_tick);

    /* wait for events and handle them */
    int active_count = _WAIT_FOR_EVENTS_;
    if (active_count < 0) return -1;

    if (active_count > 0) {
        for (int i = 0; i < active_count; i++) {
            if (_EVENTERROR_(i)) {
                /* errors are hendled as disconnections (on_close) */
                reactor_close(reactor, _GETFD_(i));
            } else {
                /* no error, then it's an active event(s) */
                if (_EVENTREADY_(i) && reactor->on_ready)
                    reactor->on_ready(reactor, _GETFD_(i));
                if (_EVENTDATA_(i) && reactor->on_data)
                    reactor->on_data(reactor, _GETFD_(i));
            }
        }
    }
    return active_count;
}

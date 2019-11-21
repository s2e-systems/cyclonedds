#include "os_defs.h"
#include "os_init.h"
#include "os_heap.h"
#include "os_cond.h"
#include "os_mutex.h"
#include "os_thread.h"
#include "os_atomics.h"
#include "os_process.h"
#include "ut_timed_cb.h"




struct ut_timed_dispatcher_t
{
    os_boolean active;
    void *listener;
};

struct ut__queue_event_t
{
    struct ut__queue_event_t *next;
    struct ut__queue_event_t *prev;
    struct ut_timed_dispatcher_t *dispatcher;
    ut_timed_cb_t callback;
    os_timeW trigger_time;
    void *arg;
};


static os_mutex g_lock;
static os_cond  g_cond;
static struct ut__queue_event_t *g_first_event = NULL;
static os_threadId g_thread_id;
static os_boolean g_terminate = OS_FALSE;


static void
ut__timed_cb_fini(void)
{
    if (os_threadIdToInteger(g_thread_id) != os_threadIdToInteger(OS_THREAD_ID_NONE)) {
        g_terminate = OS_TRUE;
        os_condSignal(&g_cond);
        (void)os_threadWaitExit(g_thread_id, NULL);
    }
    os_condDestroy(&g_cond);
    os_mutexDestroy(&g_lock);
    os_osExit();
}


static void
ut__timed_cb_init(void)
{
    static pa_uint32_t init_cnt = PA_UINT32_INIT(0);
    static os_boolean initialized = OS_FALSE;

    if (pa_inc32_nv(&init_cnt) == 1) {
        /* Initialization. */
        os_osInit();
        g_thread_id = OS_THREAD_ID_NONE;
        (void)os_mutexInit(&g_lock, NULL);
        (void)os_condInit (&g_cond, &g_lock, NULL);
        os_procAtExit(ut__timed_cb_fini);
        initialized = OS_TRUE;
    } else {
        /* Wait until we have locking. */
        while (initialized == OS_FALSE) {
            ospl_os_sleep(10 * OS_DURATION_MILLISECOND);
        }
        /* Make sure we never wrap. */
        pa_dec32(&init_cnt);
    }
}


/* Return next event. */
static struct ut__queue_event_t*
ut__remove_event(
        struct ut__queue_event_t *event)
{
    struct ut__queue_event_t *next = event->next;
    struct ut__queue_event_t *prev = event->prev;
    if (prev == NULL) {
        g_first_event = next;
    } else {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }
    os_free(event);
    return next;
}


static void*
ut__timed_dispatcher_thread(
        void *a)
{
    struct ut__queue_event_t *event;
    os_duration timeout;

    OS_UNUSED_ARG(a);

    os_mutexLock(&g_lock);

    event = g_first_event;
    do {
        if (event) {
            /* Just some sanity checks. */
            assert(event->callback);
            assert(event->dispatcher);

            /* Determine the trigger timeout of this callback. */
            timeout = os_timeWDiff(event->trigger_time, os_timeWGet());

            /* Check if this event has to be triggered. */
            if (timeout <= 0) {
                /* Trigger callback when related dispatcher is active. */
                if (event->dispatcher->active) {
                    event->callback(event->dispatcher,
                                    UT_TIMED_CB_KIND_TIMEOUT,
                                    event->dispatcher->listener,
                                    event->arg);

                    /* Remove handled event from queue, continue with next. */
                    event = ut__remove_event(event);
                } else {
                    /* Check next event. */
                    event = event->next;
                }
            } else {
                /* All following events are in the future.
                 * Let the condition wait until that future is reached. */
            }
        } else {
            /* No (more) events. Wait until some are added or until the end. */
            timeout = OS_DURATION_INFINITE;
        }

        if ((timeout > 0) || (timeout == OS_DURATION_INFINITE)) {
            /* Wait for new event, timeout or the end. */
            (void)os_condTimedWait(&g_cond, &g_lock, timeout);

            /* Restart, because 'old' triggers could have been added. */
            event = g_first_event;
        }

    } while (!g_terminate);

    os_mutexUnlock(&g_lock);

    return 0;
}


struct ut_timed_dispatcher_t*
ut_timed_dispatcher_new()
{
    struct ut_timed_dispatcher_t *d;

    /* Initialization. */
    ut__timed_cb_init();

    /* New dispatcher. */
    d = os_malloc(sizeof(struct ut_timed_dispatcher_t));
    memset(d, 0, sizeof(struct ut_timed_dispatcher_t));

    return d;
}

void
ut_timed_dispatcher_free(
        struct ut_timed_dispatcher_t *d)
{
    struct ut__queue_event_t *event;

    assert(d);

    /* Remove d related events from queue. */
    os_mutexLock(&g_lock);
    event = g_first_event;
    while (event != NULL) {
        if (event->dispatcher == d) {
            event->callback(event->dispatcher,
                            UT_TIMED_CB_KIND_DELETE,
                            NULL,
                            event->arg);
            event = ut__remove_event(event);
        } else {
            event = event->next;
        }
    }
    os_mutexUnlock(&g_lock);

    /* Free this dispatcher. */
    os_free(d);
}


void
ut_timed_dispatcher_enable(
        struct ut_timed_dispatcher_t *d, void *listener)
{
    assert(d);
    assert(!(d->active));

    os_mutexLock(&g_lock);

    /* Remember possible listener and activate. */
    d->listener = listener;
    d->active = OS_TRUE;

    /* Start thread when not running, otherwise wake it up to
     * trigger callbacks that were (possibly) previously added. */
    if (os_threadIdToInteger(g_thread_id) == os_threadIdToInteger(OS_THREAD_ID_NONE)) {
        os_threadAttr attr;
        os_threadAttrInit(&attr);
        (void)os_threadCreate (&g_thread_id, "security_dispatcher", &attr, ut__timed_dispatcher_thread, NULL);
    } else {
        os_condSignal (&g_cond);
    }

    os_mutexUnlock(&g_lock);
}


void
ut_timed_dispatcher_disable(
        struct ut_timed_dispatcher_t *d)
{
    assert(d);
    assert(!(d->active));

    os_mutexLock(&g_lock);

    /* Forget listener and deactivate. */
    d->listener = NULL;
    d->active = OS_TRUE;

    os_mutexUnlock(&g_lock);
}


void
ut_timed_dispatcher_add(
        struct ut_timed_dispatcher_t *d,
        ut_timed_cb_t cb,
        os_timeW trigger_time,
        void *arg)
{
    struct ut__queue_event_t *event_new;
    struct ut__queue_event_t *event_wrk;

    assert(d);
    assert(cb);

    /* Create event. */
    event_new = os_malloc(sizeof(struct ut__queue_event_t));
    memset(event_new, 0, sizeof(struct ut__queue_event_t));
    event_new->trigger_time = trigger_time;
    event_new->dispatcher = d;
    event_new->callback = cb;
    event_new->arg = arg;

    /* Insert event based on trigger_time. */
    os_mutexLock(&g_lock);
    if (g_first_event) {
        struct ut__queue_event_t *last;
        for (event_wrk = g_first_event; event_wrk != NULL; event_wrk = event_wrk->next) {
            last = event_wrk;
            if (os_timeWCompare(event_wrk->trigger_time, event_new->trigger_time) == OS_MORE) {
                /* This is the first event encountered that takes longer to trigger.
                 * Put our new event in front of it. */
                event_new->prev = event_wrk->prev;
                event_new->next = event_wrk;
                if (event_wrk->prev) {
                    /* Insert. */
                    event_wrk->prev->next = event_new;
                } else {
                    /* Prepend. */
                    g_first_event = event_new;
                }
                event_wrk->prev = event_new;
                break;
            }
        }
        if (event_wrk == NULL) {
            /* Append. */
            last->next = event_new;
            event_new->prev = last;
        }
    } else {
        /* This new event is the first one. */
        g_first_event = event_new;
    }
    os_mutexUnlock(&g_lock);

    /* Wake up thread (if it's running). */
    os_condSignal(&g_cond);
}




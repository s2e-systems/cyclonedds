// #include "os_defs.h"
// #include "os_init.h"
// #include "os_heap.h"
// #include "os_cond.h"
// #include "os_mutex.h"
// #include "os_thread.h"
// #include "os_atomics.h"
// #include "os_process.h"

#include <string.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/time.h"

#include "dds/security/core/ut_timed_cb.h"


struct ut_timed_dispatcher_t
{
    bool active;
    void *listener;
};

struct ut__queue_event_t
{
    struct ut__queue_event_t *next;
    struct ut__queue_event_t *prev;
    struct ut_timed_dispatcher_t *dispatcher;
    ut_timed_cb_t callback;
    dds_time_t trigger_time;
    void *arg;
};


static ddsrt_mutex_t g_lock;
static ddsrt_cond_t  g_cond;
static struct ut__queue_event_t *g_first_event = NULL;
static ddsrt_thread_t* g_thread_ptr;
static bool g_terminate = false;
static ddsrt_atomic_uint32_t init_cnt = DDSRT_ATOMIC_UINT32_INIT(0);

static void
ut__timed_cb_fini(void* a)
{
    DDSRT_UNUSED_ARG(a);

    if (g_thread_ptr != NULL /*OS_THREAD_ID_NONE*/ ){  // TODO: Check definition of thread none
        g_terminate = true;
        ddsrt_cond_signal(&g_cond);
        ddsrt_thread_join(*g_thread_ptr, NULL);
        ddsrt_free(g_thread_ptr);
        g_thread_ptr = NULL;
    }
    ddsrt_atomic_dec32(&init_cnt);
    ddsrt_cond_destroy(&g_cond);
    ddsrt_mutex_destroy(&g_lock);
    //os_osExit(); // TODO: Check any further clean-up steps
}


static void
ut__timed_cb_init(void)
{
    static bool initialized = false;

    if (ddsrt_atomic_inc32_nv(&init_cnt) == 1) {
        /* Initialization. */
        //os_osInit(); /*TODO: Check needed init steps */
        g_thread_ptr = NULL; /*OS_THREAD_ID_NONE*/; // TODO: Check definition of thread none
        ddsrt_mutex_init(&g_lock);
        ddsrt_cond_init(&g_cond);
        initialized = true;
    } else {
        /* Wait until we have locking. */
        while (initialized == false) {
            dds_sleepfor(DDS_MSECS(10));
        }
        /* Make sure we never wrap. */
        ddsrt_atomic_dec32(&init_cnt);
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
    
    ddsrt_free(event);
    return next;
}


static uint32_t
ut__timed_dispatcher_thread(
        void *a)
{
    struct ut__queue_event_t *event;
    dds_duration_t timeout;

    DDSRT_UNUSED_ARG(a);

    ddsrt_mutex_lock(&g_lock);

    event = g_first_event;
    do {
        if (event) {
            // /* Just some sanity checks. */
            // assert(event->callback);
            // assert(event->dispatcher);

            /* Determine the trigger timeout of this callback. */
            timeout = event->trigger_time - dds_time();

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
            timeout = DDS_INFINITY;
        }

        if ((timeout > 0) || (timeout == DDS_INFINITY)) {
            /* Wait for new event, timeout or the end. */
            (void)ddsrt_cond_waitfor(&g_cond, &g_lock, timeout);

            /* Restart, because 'old' triggers could have been added. */
            event = g_first_event;
        }

    } while (!g_terminate);

    ddsrt_mutex_unlock(&g_lock);

    return 0;
}


struct ut_timed_dispatcher_t*
ut_timed_dispatcher_new()
{
    struct ut_timed_dispatcher_t *d;

    /* Initialization. */
    ut__timed_cb_init();

    /* New dispatcher. */
    d = ddsrt_malloc(sizeof(struct ut_timed_dispatcher_t));
    memset(d, 0, sizeof(struct ut_timed_dispatcher_t));

    return d;
}

void
ut_timed_dispatcher_free(
        struct ut_timed_dispatcher_t *d)
{
    struct ut__queue_event_t *event;

    // assert(d);

    /* Remove d related events from queue. */
    ddsrt_mutex_lock(&g_lock);
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
    ddsrt_mutex_unlock(&g_lock);

    /* Free this dispatcher. */
    ddsrt_free(d);
}


void
ut_timed_dispatcher_enable(
        struct ut_timed_dispatcher_t *d, void *listener)
{
    // assert(d);
    // assert(!(d->active));

    ddsrt_mutex_lock(&g_lock);

    /* Remember possible listener and activate. */
    d->listener = listener;
    d->active = true;

    /* Start thread when not running, otherwise wake it up to
     * trigger callbacks that were (possibly) previously added. */
    if (g_thread_ptr == NULL /*os_threadIdToInteger(OS_THREAD_ID_NONE)*/) { // TODO: Check definition of thread none
        g_thread_ptr = ddsrt_malloc(sizeof(*g_thread_ptr));
        ddsrt_threadattr_t attr;
        ddsrt_threadattr_init(&attr);
        ddsrt_thread_create(g_thread_ptr, "security_dispatcher", &attr, ut__timed_dispatcher_thread, NULL); /* TODO: Check return an thread_id input */
        ddsrt_thread_cleanup_push(&ut__timed_cb_fini, NULL); // TODO: Revisit thread/process difference
    } else {
        ddsrt_cond_signal(&g_cond);
    }

    ddsrt_mutex_unlock(&g_lock);
}


void
ut_timed_dispatcher_disable(
        struct ut_timed_dispatcher_t *d)
{
    // assert(d);
    // assert(!(d->active));

    ddsrt_mutex_lock(&g_lock);

    /* Forget listener and deactivate. */
    d->listener = NULL;
    d->active = true;

    ddsrt_mutex_unlock(&g_lock);
}


void
ut_timed_dispatcher_add(
        struct ut_timed_dispatcher_t *d,
        ut_timed_cb_t cb,
        dds_time_t trigger_time,
        void *arg)
{
    struct ut__queue_event_t *event_new;
    struct ut__queue_event_t *event_wrk;

    // assert(d);
    // assert(cb);

    /* Create event. */
    event_new = ddsrt_malloc(sizeof(struct ut__queue_event_t));
    memset(event_new, 0, sizeof(struct ut__queue_event_t));
    event_new->trigger_time = trigger_time;
    event_new->dispatcher = d;
    event_new->callback = cb;
    event_new->arg = arg;

    /* Insert event based on trigger_time. */
    ddsrt_mutex_lock(&g_lock);
    if (g_first_event) {
        struct ut__queue_event_t *last;
        for (event_wrk = g_first_event; event_wrk != NULL; event_wrk = event_wrk->next) {
            last = event_wrk;
            if (event_wrk->trigger_time - event_new->trigger_time > 0) {
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
    ddsrt_mutex_unlock(&g_lock);

    /* Wake up thread (if it's running). */
    ddsrt_cond_signal(&g_cond);
}




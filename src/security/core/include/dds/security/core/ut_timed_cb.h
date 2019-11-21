#ifndef UT_TIMED_CALLBACK_H
#define UT_TIMED_CALLBACK_H

#include "dds/export.h"
#include "dds/ddsrt/time.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * The dispatcher that will trigger the timed callbacks.
 */
struct ut_timed_dispatcher_t;



/**
 * The callback is triggered by two causes:
 * 1. The trigger timeout has been reached.
 * 2. The related dispatcher is being deleted.
 */
typedef enum {
    UT_TIMED_CB_KIND_TIMEOUT,
    UT_TIMED_CB_KIND_DELETE
} ut_timed_cb_kind;



/**
 * Template for the timed callback functions.
 * It is NOT allowed to call any t_timed_cb API functions from within this
 * callback context.
 *
 * This will be called when the trigger time of the added callback is reached,
 * or if the related dispatcher is deleted. The latter can be used to clean up
 * possible callback resources.
 *
 * @param d             Related dispatcher.
 * @param kind          Triggered by cb timeout or dispatcher deletion.
 * @param listener      Listener that was provided when enabling the related
 *                      dispatcher (NULL with a deletion trigger).
 * @param arg           User data, provided when adding a callback to the
 *                      related dispatcher.
 *
 * @return              void
 */
typedef void
(*ut_timed_cb_t) (
        struct ut_timed_dispatcher_t *d,
        ut_timed_cb_kind kind,
        void *listener,
        void *arg);



/**
 * Create a new dispatcher for timed callbacks.
 * The dispatcher is not enabled (see ut_timed_dispatcher_enable).
 *
 * @param               void
 *
 * @return              New (disabled) timed callbacks dispatcher.
 */
DDS_EXPORT struct ut_timed_dispatcher_t*
ut_timed_dispatcher_new(void);

/**
 * Frees the given dispatcher.
 * If the dispatcher contains timed callbacks, then these will be
 * triggered with UT_TIMED_CB_KIND_DELETE and then removed. This
 * is done whether the dispatcher is enabled or not.
 *
 * @param d             The dispatcher to free.
 *
 * @return              void
 */
DDS_EXPORT void
ut_timed_dispatcher_free(
        struct ut_timed_dispatcher_t *d);



/**
 * Enables a dispatcher for timed callbacks.
 *
 * Until a dispatcher is enabled, no UT_TIMED_CB_KIND_TIMEOUT callbacks will
 * be triggered.
 * As soon as it is enabled, possible stored timed callbacks that are in the
 * past will be triggered at that moment.
 * Also, from this point on, possible future callbacks will also be triggered
 * when the appropriate time has been reached.
 *
 * A listener argument can be supplied that is returned when the callback
 * is triggered. The dispatcher doesn't do anything more with it, so it may
 * be NULL.
 *
 * UT_TIMED_CB_KIND_DELETE callbacks will always be triggered despite the
 * dispatcher being possibly disabled.
 *
 * @param d             The dispatcher to enable.
 * @param listener      An object that is returned with the callback.
 *
 * @return              void.
 */
DDS_EXPORT void
ut_timed_dispatcher_enable(
        struct ut_timed_dispatcher_t *d,
        void *listener);



/**
 * Disables a dispatcher for timed callbacks.
 *
 * When a dispatcher is disabled (default after creation), it will not
 * trigger any related callbacks. It will still store them, however, so
 * that they can be triggered after a (re)enabling.
 *
 * This is when the callback is actually triggered by a timeout and thus
 * its kind is UT_TIMED_CB_KIND_TIMEOUT. UT_TIMED_CB_KIND_DELETE callbacks
 * will always be triggered despite the dispatcher being possibly disabled.
 *
 * @param d             The dispatcher to disable.
 *
 * @return              void.
 */
DDS_EXPORT void
ut_timed_dispatcher_disable(
        struct ut_timed_dispatcher_t *d);



/**
 * Adds a timed callback to a dispatcher.
 *
 * The given callback will be triggered with UT_TIMED_CB_KIND_TIMEOUT when:
 *  1. The dispatcher is enabled and
 *  2. The trigger_time has been reached.
 *
 * If the trigger_time lays in the past, then the callback is still added.
 * When the dispatcher is already enabled, it will trigger this 'past'
 * callback immediately. Otherwise, the 'past' callback will be triggered
 * at the moment that the dispatcher is enabled.
 *
 * The given callback will be triggered with UT_TIMED_CB_KIND_DELETE when:
 *  1. The related dispatcher is deleted (ignoring enable/disable).
 *
 * This is done so that possible related callback resources can be freed.
 *
 * @param d             The dispatcher to add the callback to.
 * @param cb            The actual callback function.
 * @param trigger_time  A wall-clock time of when to trigger the callback.
 * @param arg           User data that is provided with the callback.
 *
 * @return              void.
 */
DDS_EXPORT void
ut_timed_dispatcher_add(
        struct ut_timed_dispatcher_t *d,
        ut_timed_cb_t cb,
        dds_time_t trigger_time,
        void *arg);

#if defined (__cplusplus)
}
#endif

#endif /* UT_TIMED_CALLBACK_H */

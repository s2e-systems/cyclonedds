#include <Winsock2.h>
#include <iphlpapi.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/threads.h"

#include "dds/ddsrt/ip_change.h"


struct ddsrt_ip_change_notify_data
{
    ddsrt_thread_t thread;
    dds_ip_change_notify_callback cb;
    void *data;
};


static uint32_t ip_change_notify_thread(void* context)
{
    struct ddsrt_ip_change_notify_data *icn = (struct ddsrt_ip_change_notify_data *)context;
    while(1)
    {
        if (NotifyAddrChange(NULL, NULL) == NO_ERROR)
        {
            icn->cb();
        }
        else
        {
            DDS_INFO("NotifyAddrChange error...%d\n", WSAGetLastError());
        }
    }
    return 0;
}


struct ddsrt_ip_change_notify_data *ddsrt_ip_change_notify_new(dds_ip_change_notify_callback cb, void *data)
{
    struct ddsrt_ip_change_notify_data *icnd = (struct ddsrt_ip_change_notify_data *)ddsrt_malloc(sizeof(*icnd));
    ddsrt_threadattr_t attr;
    dds_return_t osres;

    icnd->cb = cb;
    icnd->data = data;
    ddsrt_threadattr_init(&attr);

    osres = ddsrt_thread_create(&icnd->thread, "ip_change_notify", &attr, ip_change_notify_thread, (void*)icnd);
    if (osres != DDS_RETCODE_OK)
    {
        DDS_FATAL("Cannot create thread of ip change notify");
    }

    return icnd;
}

void ddsrt_ip_change_notify_free(struct ddsrt_ip_change_notify_data* icnd)
{
    ddsrt_thread_join(icnd->thread, NULL);
    ddsrt_free(icnd);
}

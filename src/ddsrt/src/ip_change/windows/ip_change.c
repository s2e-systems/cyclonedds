#include <Winsock2.h>
#include <iphlpapi.h>
#include <signal.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/string.h"

#include "dds/ddsrt/ip_change.h"


struct ddsrt_ip_change_notify_data
{
    ddsrt_thread_t thread;
    dds_ip_change_notify_callback cb;
    void* data;
    const char* if_name;
    ddsrt_ifaddrs_t* ifa_root_previous;
    volatile sig_atomic_t termflag;
};


static uint32_t ip_change_notify_thread(void* context)
{
    
    HANDLE hand = NULL;
    OVERLAPPED overlap;
    DWORD wait_ret;
    bool found = false;
    overlap.hEvent = WSACreateEvent();
    dds_return_t ret;
    ddsrt_ifaddrs_t* ifa_root, * ifa, * ifa_previous;
    const int afs[] = { AF_INET, DDSRT_AF_TERM };

    struct ddsrt_ip_change_notify_data *icn = (struct ddsrt_ip_change_notify_data *)context;

    if (NotifyAddrChange(&hand, &overlap) != NO_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            DDS_ERROR("Error %d creating address change notification\n", WSAGetLastError());
        }
        
    }

    while(!icn->termflag)
    {
        wait_ret = WaitForSingleObject(overlap.hEvent, 1000 /*dwMilliseconds*/);
        if (wait_ret == WAIT_OBJECT_0)
        {
            // Manually reset the event and notify for the next possible change
            WSAResetEvent(overlap.hEvent);
            NotifyAddrChange(&hand, &overlap);

            // Retrieve the list of addresses
            ret = ddsrt_getifaddrs(&ifa_root, afs);
            if (ret != DDS_RETCODE_OK)
            {
                DDS_ERROR("Error retrieving interface addresses\n");
            }

            // Go through the address list and find the address
            // for the interface being monitored. If the address changed trigger the callback
            for (ifa = ifa_root; ifa; ifa = ifa->next)
            {
                if (ddsrt_strcasecmp(ifa->name, icn->if_name) == 0)
                {
                    found = false;
                    for (ifa_previous = icn->ifa_root_previous; ifa_previous; ifa_previous = ifa_previous->next)
                    {
                        if (ddsrt_strcasecmp(ifa->name, ifa_previous->name) == 0 &&
                            memcmp(ifa->addr, ifa_previous->addr, sizeof(ifa->addr)) == 0 &&
                            memcmp(ifa->netmask, ifa_previous->netmask, sizeof(ifa->netmask)) == 0)
                        {
                            found = true;
                            break;
                        }
                    }


                    if (!found)
                    {
                        icn->cb(icn->data);
                        break;
                    }
                }
            }

            ddsrt_freeifaddrs(icn->ifa_root_previous);
            icn->ifa_root_previous = ifa_root;
            ifa_root = NULL;
        }
        else if (wait_ret != WAIT_TIMEOUT)
        {
            DDS_ERROR("NotifyAddrChange error...%d\n", WSAGetLastError());
        }
    }
    CancelIPChangeNotify(&overlap);
    ddsrt_freeifaddrs(icn->ifa_root_previous);
    return 0;
}


struct ddsrt_ip_change_notify_data *ddsrt_ip_change_notify_new(const char* if_name, dds_ip_change_notify_callback cb, void *data)
{
    struct ddsrt_ip_change_notify_data *icnd = (struct ddsrt_ip_change_notify_data *)ddsrt_malloc(sizeof(*icnd));
    ddsrt_threadattr_t attr;
    dds_return_t osres;
    dds_return_t ret;
    const int afs[] = { AF_INET, DDSRT_AF_TERM };

    icnd->cb = cb;
    icnd->data = data;
    icnd->if_name = if_name;
    ddsrt_threadattr_init(&attr);
    icnd->termflag = 0;

    // Retrieve the original list of addresses
    ret = ddsrt_getifaddrs(&icnd->ifa_root_previous, afs);
    if (ret != DDS_RETCODE_OK)
    {
        DDS_ERROR("Error retrieving interface addresses\n");
    }

    osres = ddsrt_thread_create(&icnd->thread, "ip_change_notify", &attr, ip_change_notify_thread, (void*)icnd);
    if (osres != DDS_RETCODE_OK)
    {
        DDS_FATAL("Cannot create thread of ip change notify");
    }

    return icnd;
}

void ddsrt_ip_change_notify_free(struct ddsrt_ip_change_notify_data* icnd)
{
    icnd->termflag = 1;
    ddsrt_thread_join(icnd->thread, NULL);
    ddsrt_free(icnd);
}

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
    volatile sig_atomic_t termflag;
};


static uint32_t ip_change_notify_thread(void* context)
{
    dds_return_t ret;
    ddsrt_ifaddrs_t* ifa_root_previous, * ifa_root, * ifa, * ifa_previous;
    const int afs[] = { AF_INET, DDSRT_AF_TERM };
    //struct sockaddr addr;
    //struct sockaddr netmask;

    struct ddsrt_ip_change_notify_data *icn = (struct ddsrt_ip_change_notify_data *)context;

    // Retrieve the list of addresses
    ret = ddsrt_getifaddrs(&ifa_root_previous, afs);
    if (ret != DDS_RETCODE_OK)
    {
        DDS_ERROR("Error retrieving interface addresses\n");
    }

    //printf("Searching for %s\n", icn->if_name);
    //// Go through the address list and find the original address
    //// for the interface being monitored
    //for (ifa = ifa_root; ifa; ifa = ifa->next)
    //{
    //    printf("Interface %s\n", ifa->name);
    //    if (ddsrt_strcasecmp(ifa->name, icn->if_name) == 0)
    //    {
    //        char buf[256];
    //        
    //        memcpy(&addr, ifa->addr, sizeof(struct sockaddr));
    //        ddsrt_sockaddrtostr(&addr, buf, 256);
    //        printf("Found with address %s \n", buf);
    //        memcpy(&netmask, ifa->netmask, sizeof(struct sockaddr));
    //        break;
    //    }
    //}
    

    while(!icn->termflag)
    {
        if (NotifyAddrChange(NULL, NULL) == NO_ERROR)
        {
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
                    bool found = false;

                    for (ifa_previous = ifa_root_previous; ifa_previous; ifa_previous = ifa_previous->next)
                    {
                        if (ddsrt_strcasecmp(ifa->name, ifa_previous->name) == 0 &&
                            memcmp(ifa->addr, ifa_previous->addr, sizeof(struct sockaddr)) == 0 && 
                            memcmp(ifa->netmask, ifa_previous->netmask, sizeof(struct sockaddr)) == 0)
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
            
        }
        else
        {
            DDS_INFO("NotifyAddrChange error...%d\n", WSAGetLastError());
        }
    }
    ddsrt_freeifaddrs(ifa_root);
    ddsrt_freeifaddrs(ifa_root_previous);
    return 0;
}


struct ddsrt_ip_change_notify_data *ddsrt_ip_change_notify_new(dds_ip_change_notify_callback cb, const char* if_name, void *data)
{
    struct ddsrt_ip_change_notify_data *icnd = (struct ddsrt_ip_change_notify_data *)ddsrt_malloc(sizeof(*icnd));
    ddsrt_threadattr_t attr;
    dds_return_t osres;

    icnd->cb = cb;
    icnd->data = data;
    icnd->if_name = if_name;
    ddsrt_threadattr_init(&attr);
    icnd->termflag = 0;

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

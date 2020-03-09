#include "dds/ddsrt/ip_change.h"

#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/time.h"



struct ddsrt_ip_change_notify_data
{
    ddsrt_thread_t thread;
    dds_ip_change_notify_callback cb;
    void *data;
};


static uint32_t ip_change_notify_thread(void* context)
{
    struct ddsrt_ip_change_notify_data *icn = (struct ddsrt_ip_change_notify_data *)context;

    struct sockaddr_nl addr;
    int sock, len;
    char buffer[4096];
    struct nlmsghdr *nlh;

    if ((sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
        perror("couldn't open NETLINK_ROUTE socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_IFADDR;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("couldn't bind");
        return 1;
    }

    nlh = (struct nlmsghdr *)buffer;
    while ((len = recv(sock, nlh, 4096, 0)) > 0) {
        while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
            if (nlh->nlmsg_type == RTM_NEWADDR) {
                struct ifaddrmsg *ifa = (struct ifaddrmsg *) NLMSG_DATA(nlh);
                struct rtattr *rth = IFA_RTA(ifa);
                int rtl = IFA_PAYLOAD(nlh);

                while (rtl && RTA_OK(rth, rtl)) {
                    if (rth->rta_type == IFA_LOCAL) {
                        char name[IFNAMSIZ];
                        if_indextoname(ifa->ifa_index, name);
                        char ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, RTA_DATA(rth), ip, sizeof(ip));
                        icn->cb();
                        printf("interface %s ip: %s\n", name, ip);
                        fflush(stdout);
                    }
                    rth = RTA_NEXT(rth, rtl);
                }
            }
            nlh = NLMSG_NEXT(nlh, len);
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

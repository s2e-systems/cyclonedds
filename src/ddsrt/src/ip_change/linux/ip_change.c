#include "dds/ddsrt/ip_change.h"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <net/if.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/string.h"


struct ddsrt_ip_change_notify_data
{
  ddsrt_thread_t thread;
  dds_ip_change_notify_callback cb;
  void *data;
  const char* if_name;
  int sock;
  volatile sig_atomic_t termflag;
};



static uint32_t ip_change_notify_thread(void* vicnd)
{
  struct ddsrt_ip_change_notify_data *icnd = (struct ddsrt_ip_change_notify_data *)vicnd;

  ssize_t len;
  const size_t buf_size = 4096;
  char buffer[buf_size];
  struct nlmsghdr *nlh = (struct nlmsghdr *)buffer;

  while (!icnd->termflag)
  {
    len = recv(icnd->sock, nlh, buf_size, MSG_DONTWAIT);
    if (len > 0)
    {
      while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
        if (nlh->nlmsg_type == RTM_NEWADDR || nlh->nlmsg_type == RTM_DELADDR) {
          struct ifaddrmsg *ifa = (struct ifaddrmsg *) NLMSG_DATA(nlh);
          struct rtattr *rth = IFA_RTA(ifa);
          long unsigned int rtl = IFA_PAYLOAD(nlh);

          while (rtl && RTA_OK(rth, rtl)) {
            if (rth->rta_type == IFA_LOCAL) {
              char name[IFNAMSIZ];
              if_indextoname(ifa->ifa_index, name);
              if (ddsrt_strcasecmp(name, icnd->if_name) == 0)
              {
                icnd->cb(icnd->data);
              }
            }
            rth = RTA_NEXT(rth, rtl);
          }
        }
        nlh = NLMSG_NEXT(nlh, len);
      }
    }
  }
  return 0;
}


struct ddsrt_ip_change_notify_data *ddsrt_ip_change_notify_new(const char* if_name, dds_ip_change_notify_callback cb, void *data)
{
  struct ddsrt_ip_change_notify_data *icnd = (struct ddsrt_ip_change_notify_data *)ddsrt_malloc(sizeof(*icnd));
  ddsrt_threadattr_t attr;
  dds_return_t osres;
  struct sockaddr_nl addr;

  icnd->cb = cb;
  icnd->data = data;
  icnd->if_name = if_name;
  ddsrt_threadattr_init(&attr);
  icnd->termflag = 0;


  if ((icnd->sock = socket(PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, NETLINK_ROUTE)) == -1) {
    DDS_ERROR("couldn't open NETLINK_ROUTE socket");
  }

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_groups = RTMGRP_IPV4_IFADDR;

  if (bind(icnd->sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    DDS_ERROR("couldn't bind");
  }

  osres = ddsrt_thread_create(&icnd->thread, "ip_change_notify", &attr, ip_change_notify_thread, (void*)icnd);
  if (osres != DDS_RETCODE_OK)
  {
    DDS_ERROR("Cannot create thread of ip change notify");
  }

  return icnd;
}


void ddsrt_ip_change_notify_free(struct ddsrt_ip_change_notify_data* icnd)
{
  icnd->termflag = 1;
  ddsrt_thread_join(icnd->thread, NULL);
  int ret = close(icnd->sock);
  if (ret != 0)
  {
    DDS_ERROR("Closing socket failed with: %i %s\n", errno, strerror(errno));
  }
  ddsrt_free(icnd);
}

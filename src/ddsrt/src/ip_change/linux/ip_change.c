#include "dds/ddsrt/ip_change.h"

#include <netinet/in.h>
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



struct ddsrt_ip_change_notify_data
{
  ddsrt_thread_t thread;
  dds_ip_change_notify_callback cb;
  void *data;
};

static volatile sig_atomic_t termflag = 0;

static int sock;

static uint32_t ip_change_notify_thread(void* context)
{
  struct ddsrt_ip_change_notify_data *icn = (struct ddsrt_ip_change_notify_data *)context;

  struct sockaddr_nl addr;

  ssize_t len;
  char buffer[4096];
  struct nlmsghdr *nlh;

  if ((sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
    perror("couldn't open NETLINK_ROUTE socket");
    return 1;
  }

  if(fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0) {
    perror("couldn't make socket non-blocking");
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
  while (!termflag)
  {
    len = recv(sock, nlh, 4096, 0);
    if (len > 0)
    {
      printf("here\n");
      while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE))
      {
        printf("and here\n");
        if (nlh->nlmsg_type == RTM_NEWADDR) {

          struct ifaddrmsg *ifa = (struct ifaddrmsg *) NLMSG_DATA(nlh);
          struct rtattr *rth = IFA_RTA(ifa);
          long unsigned int rtl = IFA_PAYLOAD(nlh);

          while (rtl && RTA_OK(rth, rtl))
          {
            printf("and also here\n");
            if (rth->rta_type == IFA_LOCAL)
            {
              char name[IFNAMSIZ];
              if_indextoname(ifa->ifa_index, name);
              char ip[INET_ADDRSTRLEN];
              inet_ntop(AF_INET, RTA_DATA(rth), ip, sizeof(ip));
              icn->cb(icn->data);
              printf("interface %s ip: %s\n", name, ip);
              fflush(stdout);
            }
            rth = RTA_NEXT(rth, rtl);
          }
          printf("done 3\n");
        }
        nlh = NLMSG_NEXT(nlh, len);
        printf("done 2\n");
      }
      printf("done 1\n");
    }
  }
  printf("done 0\n");
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
  printf("ddsrt_ip_change_notify_free\n");
  termflag = 1;
  int ret = close(sock);
  if (ret != 0)
  {
    printf("%i %s\n", errno, strerror(errno));
  }
  ddsrt_thread_join(icnd->thread, NULL);
  ddsrt_free(icnd);
}

#include "dds/ddsrt/ip_change.h"


#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#ifndef _WIN32
    #include <sys/ioctl.h>
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <ifaddrs.h>
#endif

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/time.h"

#include "CUnit/Test.h"

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            ddsrt_strlcpy(s, "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}

static void printIPAddress()
{
  dds_return_t ret;
  ddsrt_ifaddrs_t *ifa_root, *ifa;
  const int afs[] = { AF_INET, DDSRT_AF_TERM };
  ret = ddsrt_getifaddrs(&ifa_root, afs);
  if (ret == DDS_RETCODE_OK)
  {
    for (ifa = ifa_root; ifa; ifa = ifa->next)
    {
        char ip_addr[AF_MAX];
        get_ip_str(ifa->addr, (char*)&ip_addr, AF_MAX);
      printf("if: %s (%s)\n", ifa->name, ip_addr);
    }
  }
  ddsrt_freeifaddrs(ifa_root);
}

static void callback(void* vdata)
{
  printIPAddress();
  int* data = (int*)vdata;
  *data = 1;
}

#ifndef _WIN32
static void checkIoctlError(int e, int i)
{
  if (e == -1)
  {
    printf("At %d:", i);
    char* str = malloc(256);
    switch (errno)
    {
    case EBADF:
      str = "fd is not a valid file descriptor";
      break;
    case EFAULT:
      str = "argp references an inaccessible memory area.";
      break;
    case EINVAL:
      str = "request or argp is not valid.";
      break;
    case ENOTTY:
      str = "fd is not associated with a character special device. The specified request does not apply to the kind of object that the file descriptor fd references.";
          break;
    default:
      str = strerror(errno);
    }
    printf("ioctl error: %s\n", str);
  }
}


static void change_address(const char *ip)
{
  const char * name = "eth11";
  struct ifreq ifr;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strncpy(ifr.ifr_name, name, IFNAMSIZ);

  ifr.ifr_addr.sa_family = AF_INET;
  struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
  int r1 = inet_pton(AF_INET, ip, &addr->sin_addr);
  if (r1 != 1)
  {
    exit(-1);
  }
  int r2 = ioctl(fd, SIOCSIFADDR, &ifr);
  checkIoctlError(r2, 2);

  close(fd);
}

static void create_if()
{
  system("sudo ip link add eth11 type dummy");
}

static void delete_if()
{
  system("sudo ip link delete eth11");
}
#else

static void create_if()
{
}

static void delete_if()
{
}

static void change_address(const char *ip)
{
    DDSRT_UNUSED_ARG(ip);
}

#endif

CU_Init(ddsrt_dhcp)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_dhcp)
{
  ddsrt_fini();
  return 0;
}


CU_Test(ddsrt_ip_change_notify, ipv4)
{
  create_if();

  const char *ip_before = "10.12.0.1";
  change_address(ip_before);

  printIPAddress();

  const int expected = 1;
  int result = 0;

  struct ddsrt_ip_change_notify_data* icnd = ddsrt_ip_change_notify_new(&callback, &result);

  const char *ip_after = "10.12.0.2";
  change_address(ip_after);

  dds_sleepfor(10000000000);
  ddsrt_ip_change_notify_free(icnd);

  delete_if();

  CU_ASSERT_EQUAL(expected, result);

}


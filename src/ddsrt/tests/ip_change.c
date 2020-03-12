#include "dds/ddsrt/ip_change.h"

#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ifaddrs.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"

#include "CUnit/Test.h"

#ifdef _WIN32
#include <Winsock2.h>
#include <iphlpapi.h>
#endif

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

static void printIPAddress(void* vdata)
{
  dds_return_t ret;
  ddsrt_ifaddrs_t *ifa_root, *ifa;
  const int afs[] = { AF_INET, DDSRT_AF_TERM };
  ret = ddsrt_getifaddrs(&ifa_root, afs);
  if (ret == DDS_RETCODE_OK)
  {
    for (ifa = ifa_root; ifa; ifa = ifa->next)
    {
      if (ifa->addr->sa_family == AF_INET) {
        if (ifa->flags & IFF_LOOPBACK)
        {
          printf("lo: %s\n", ifa->name);
        }
        else
        {
          printf("if: %s\n", ifa->name);
        }
      }
    }
  }
  ddsrt_freeifaddrs(ifa_root);

  int* data = (int*)vdata;
  *data = 1;
  // The following code is from the ifaddr tests,
  // It does crash though if the address of the interface
  // is changed. This is because there is a short moment
  // when the interface has no address before it gests the
  // new one. This losing of the address causes a trigger
  // of the "IP address changed" as well.

  //    dds_return_t ret;
  //    ddsrt_ifaddrs_t *ifa_root, *ifa;
  //    const int afs[] = { AF_INET, DDSRT_AF_TERM };

  //    ret = ddsrt_getifaddrs(&ifa_root, afs);
  //    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  //    for (ifa = ifa_root; ifa; ifa = ifa->next) {
  //      CU_ASSERT_PTR_NOT_EQUAL_FATAL(ifa->addr, NULL);
  //      CU_ASSERT_EQUAL(ifa->addr->sa_family, AF_INET);
  //      if (ifa->addr->sa_family == AF_INET) {
  //        if (ifa->flags & IFF_LOOPBACK) {
  //          CU_ASSERT(ddsrt_sockaddr_isloopback(ifa->addr));
  //        } else {
  //          printf("IP adress: %s\n", ifa->addr->sa_data);
  //          CU_ASSERT(!ddsrt_sockaddr_isloopback(ifa->addr));
  //        }
  //      }
  //    }

  //    ddsrt_freeifaddrs(ifa_root);


#ifdef _WIN32
  PMIB_IPADDRTABLE pIPAddrTable;
  DWORD dwSize = 0;
  DWORD dwRetVal = 0;
  IN_ADDR IPAddr;

  pIPAddrTable = (MIB_IPADDRTABLE *) MALLOC(sizeof (MIB_IPADDRTABLE));
  if (pIPAddrTable == NULL) {
    CU_FAIL("");
  }
  else
  {
    dwSize = 0;
    // Make an initial call to GetIpAddrTable to get the
    // necessary size into the dwSize variable
    if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
    {
      FREE(pIPAddrTable);
      pIPAddrTable = (MIB_IPADDRTABLE *) MALLOC(dwSize);

    }
    if (pIPAddrTable == NULL)
    {
      CU_FAIL("");
    }
  }
  // Make a second call to GetIpAddrTable to get the
  // actual data we want
  dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0);
  if (dwRetVal == NO_ERROR)
  {
    // Save the interface index to use for adding an IP address
    for (DWORD i = 0; i < pIPAddrTable->dwNumEntries; i++)
    {
      struct _MIB_IPADDRROW_XP table = pIPAddrTable->table[i];
      IPAddr.S_un.S_addr = (u_long)table.dwAddr;
      printf("\tIP Address:       \t%s (%lu)\n", inet_ntoa(IPAddr), table.dwAddr);
      IPAddr.S_un.S_addr = (u_long)table.dwMask;
      printf("\tSubnet Mask:      \t%s (%lu)\n", inet_ntoa(IPAddr), table.dwMask);
      IPAddr.S_un.S_addr = (u_long)table.dwBCastAddr;
      printf("\tBroadCast Address:\t%s (%lu)\n\n", inet_ntoa(IPAddr), table.dwBCastAddr);
    }
  }
  else
  {
    printf("Call to GetIpAddrTable failed with error %lu.\n", dwRetVal);
    if (pIPAddrTable)
    {
      FREE(pIPAddrTable);
    }
    CU_FAIL("");
  }

  if (pIPAddrTable)
  {
    FREE(pIPAddrTable);
    pIPAddrTable = NULL;
  }
#endif
}


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


static void change_address(const char *ip, const char * name)
{
//  const char * name = "docker0";
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


//    struct sockaddr_in* addr_nm = (struct sockaddr_in*)&ifr.ifr_addr;
//    inet_pton(AF_INET, "255.255.0.0", &addr_nm->sin_addr);
//    int r3 = ioctl(fd, SIOCSIFNETMASK, &ifr);
//    checkIoctlError(r3, 3);

  close(fd);
}


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
  char* ifname;

  dds_return_t ret;
  ddsrt_ifaddrs_t *ifa_root, *ifa;
  const int afs[] = { AF_INET, DDSRT_AF_TERM };
  ret = ddsrt_getifaddrs(&ifa_root, afs);
  if (ret == DDS_RETCODE_OK)
  {
    for (ifa = ifa_root; ifa; ifa = ifa->next)
    {
      if (ifa->addr->sa_family == AF_INET) {
        if (ifa->flags & IFF_LOOPBACK)
        {
          printf("lo: %s\n", ifa->name);
        }
        else
        {
          printf("if: %s\n", ifa->name);
          if (ddsrt_strncasecmp(ifa->name, "docker", 3) != 0)
          {
            ifname = ddsrt_strdup(ifa->name);
          }
        }
      }
    }
  }
  ddsrt_freeifaddrs(ifa_root);

  printf("selected if: %s\n", ifname);
  fflush(stdout);
  int expected = 1;
  int result = 0;
  const char *ip_before = "10.12.0.1";
  change_address(ip_before, ifname);

  usleep(20000);

  struct ddsrt_ip_change_notify_data* icnd = ddsrt_ip_change_notify_new(&printIPAddress, &result);
  usleep(10000);
  const char *ip_after = "10.12.0.2";
  change_address(ip_after, ifname);

  ddsrt_free(ifname);

  usleep(10000);
  ddsrt_ip_change_notify_free(icnd);

  CU_ASSERT_EQUAL(expected, result);

}


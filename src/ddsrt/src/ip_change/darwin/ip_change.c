#include "dds/ddsrt/ip_change.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"

struct ddsrt_ip_change_notify_data
{
  /* not implemented yet */
};

struct ddsrt_ip_change_notify_data *ddsrt_ip_change_notify_new(dds_ip_change_notify_callback cb, const char* if_name, void *data)
{
  /* not implemented yet */
  DDSRT_UNUSED_ARG(cb);
  DDSRT_UNUSED_ARG(if_name);
  DDSRT_UNUSED_ARG(data);
  struct ddsrt_ip_change_notify_data *icnd = (struct ddsrt_ip_change_notify_data *)ddsrt_malloc(sizeof(*icnd));
  return icnd;
}


void ddsrt_ip_change_notify_free(struct ddsrt_ip_change_notify_data* icnd)
{
  /* not implemented yet */
  ddsrt_free(icnd);
}

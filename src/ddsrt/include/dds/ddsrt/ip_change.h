#ifndef DDSRT_IP_CHANGE_H
#define DDSRT_IP_CHANGE_H

#include "dds/export.h"

typedef void (*dds_ip_change_notify_callback)(void* );

struct ddsrt_ip_change_notify_data;

DDS_EXPORT  struct ddsrt_ip_change_notify_data *ddsrt_ip_change_notify_new(dds_ip_change_notify_callback cb, void *data);
DDS_EXPORT  void ddsrt_ip_change_notify_free(struct ddsrt_ip_change_notify_data* icnd);


#endif

#ifndef NG_DNS_H
#define NG_DNS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ev.h"

struct ngdns_base;
struct ngdns_req;
typedef void (*ngdns_cb)(const char* host, int ret, const char* ip, const char* cname, void* args);
struct ngdns_base* ngdns_base_new(struct ev_loop* loop, const char* ns_server);
struct ngdns_req* ngdns_resolve(struct ngdns_base* base,const char* host, ngdns_cb cb, void* args);
void ngdns_cancel(struct ngdns_req* req);
void ngdns_base_free(struct ngdns_base* base);
struct ngdns_base* ngdns_addr_reset(struct ngdns_base* base, const char* ns_addr);
/*
int dns_init(struct co_base* base, const char* ns_server);
const char* dns_resolve(const char* host);
int dns_fini();
void dns_cancel_all();
*/
#ifdef __cplusplus
}
#endif
#endif


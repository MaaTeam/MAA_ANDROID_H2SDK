
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ngdns.h"
#include "list.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/time.h>
//#include "utils.h"

#ifdef ANDROID
#include <android/log.h>
#endif  // ANDROID

#define LOG(buf) log_android(buf, __LINE__);

void log_android(char* buf, int line ) {
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "MATO", "%d::%s", line, buf);
#endif
}

#define CACHE_HASH_BUCKET_SIZE 4096
#define HASH_BUCKET_SIZE 512
#define WORKSPACE_SIZE 512
#define NAME_SIZE      16 
struct ngdns_base;
struct cache_record {
    list_head list;
    char cname[256];
    char host[256];
    char ip[64];
    int64_t ts_timeout;
};

struct request_record {
    char host[256];
};

struct response_record {
    int type;
    char ip[64];
    char cname[256];
    char host[256];
    int ttl;
};

struct ngdns_req {
    
    list_head list;
    list_head list_pending_item;
    int task_id;
    bool is_ip;
    struct dns_real_req* real_req;
    ngdns_cb cb;
    struct ev_timer tmr_activate;
    char host[256];
    char cname[256];
    char ip[64];
    struct ngdns_base* base;
    bool is_canceled;
    void* args;
};

struct dns_real_req {
    struct ngdns_base* base;
    list_head list;
    list_head list_reqs;
    bool pending;
    char host[256];
    int count_timeout;
    struct ev_timer tmr_retry;
    //event* tmr_retry;
};


struct ngdns_base {
    struct ev_loop* loop;
    int s_fd;
    struct ev_io s_event;
    char s_ns_addr[255];
    list_head list_head_pending_items;
    list_head s_hash[HASH_BUCKET_SIZE];

    list_head s_cache_hash[CACHE_HASH_BUCKET_SIZE];
    struct sockaddr_in s_server_addr;
};



static void dns_real_req_start(struct dns_real_req* real_req);

static void dns_recv_cb(struct ev_loop *loop, ev_io *w, int revents);

static int dns_send_req(struct ngdns_base* base, const char* host);

static void dns_real_req_active_and_free(struct ngdns_base* base, 
                                         struct dns_real_req* req, 
                                         struct response_record* resps,
                                         int resp_count,
                                         int ret_code);

static int dns_hash_str(const char* str) {
    uint64_t total = 0;
    while(*str) {
        total += *str;
        str++;
    }
    return total % HASH_BUCKET_SIZE;
}

static int dns_hash_cache_str(const char* str) {
    uint64_t total = 0;
    while(*str) {
        total += *str;
        str++;
    }
    return total % CACHE_HASH_BUCKET_SIZE;
}

static int64_t get_ms_now() {
  int64_t t = 1;
  struct timeval val = {};
  gettimeofday(&val, NULL);
  return t * val.tv_sec * 1000 + val.tv_usec /1000;
}

static struct cache_record* dns_cache_find(struct ngdns_base* base, const char* host) {
    LOG("dns_cache_find");
    int i = dns_hash_cache_str(host);
    struct cache_record* pos = NULL;
    struct cache_record* res = NULL;
    list_for_each_entry(pos, &base->s_cache_hash[i], list) {
        if(strcmp(pos->host, host) == 0) {
            res = pos;
        }
    }

    if(!res) {
        return NULL;
    }

    if(res->ts_timeout > get_ms_now()) {
        return res;
    } else {
        fprintf(stderr,"%s dns cache timeout, deleting\n", host);
        list_del(&res->list);
        return NULL;
    }    
}

static void dns_cache_add(struct ngdns_base* base, const char* host, const char* ip, const char* cname, int ttl) {
    LOG("dns_cache_add");
    if(dns_cache_find(base, host) != NULL) {
        return;
    }
    int i = dns_hash_cache_str(host);
    struct cache_record* record = (struct cache_record*)calloc(1, sizeof(struct cache_record));
    strncpy(record->host, host, sizeof(record->host));
    strncpy(record->ip, ip, sizeof(record->ip));
    strncpy(record->cname, cname, sizeof(record->ip));
    record->ts_timeout = get_ms_now() + ttl * 1000;
    list_add_tail(&record->list, &base->s_cache_hash[i]);
}

void ngdns_cancel(struct ngdns_req* req) {
    char buf[256] = {0};
    snprintf(buf, 256, "canceling realreq=%p\n", req->real_req);
    LOG(buf);
    if(req->real_req != NULL) {
    snprintf(buf, 256, "&req->list=%p\n", &req->list);
    LOG(buf);
    snprintf(buf, 256, "&req->list->prev=%p\n", (&req->list)->prev);
    LOG(buf);
    snprintf(buf, 256, "&req->list->next=%p\n", (&req->list)->next);
    LOG(buf);
    list_del(&req->list);

    snprintf(buf, 256, "&req->real_req->list_reqs=%p\n", &req->real_req->list_reqs);
    LOG(buf);

        if(list_empty(&req->real_req->list_reqs)) {
            snprintf(buf, 256, "&req->real_req->list=%p\n", &req->real_req->list);
            LOG(buf);
            list_del(&(req->real_req->list));
            LOG(buf);
            ev_timer_stop(req->base->loop, &req->real_req->tmr_retry);
            free(req->real_req);
        }       

    } else {
        list_del(&req->list_pending_item);
        ev_timer_stop(req->base->loop, &req->tmr_activate);
    }
    
    LOG("xxxxxxxxxxxxxxxxx");
    free(req);
}

static void dns_real_req_timercb(struct ev_loop *loop, ev_timer *w,
                                 int revents) {
    LOG("dns_real_req_timercb");
    struct dns_real_req* real_req = (struct dns_real_req*)w->data;
    real_req->count_timeout ++;

    fprintf(stderr, "host %s resolve timeout\n", real_req->host);
   if(real_req->count_timeout >= 3) {
        dns_real_req_active_and_free(real_req->base, real_req, NULL, 0, -1);
    } else {
       dns_real_req_start(real_req);
    }
}

static void dns_req_activatecb(struct ev_loop *loop, ev_timer *w,
                                 int revents) {
    LOG("dns_req_activatecb");

    struct ngdns_req* req = (struct ngdns_req*)w->data;
    ev_timer_stop(req->base->loop, &req->tmr_activate);
    list_del(&(req->list_pending_item));
    req->cb(req->host, 0, req->ip, req->cname, req->args);

    LOG("before free req");
    free(req);
    LOG("after free req");
    //
}

static struct dns_real_req* dns_real_req_find(struct ngdns_base* base, const char* host) {
    LOG("dns_real_req_find");
    int pos = dns_hash_str(host);
    struct dns_real_req* p = NULL;
    list_for_each_entry(p, &base->s_hash[pos], list) {
        if(strncmp(p->host, host, sizeof(p->host)) == 0) { 
            return p;
        }
    }
    return NULL;
}

static void dns_real_req_add(struct dns_real_req* real_req) {
    LOG("dns_real_req_add");
    int pos = dns_hash_str(real_req->host);
    list_add(&real_req->list, &real_req->base->s_hash[pos]);
}

static void dns_real_req_start(struct dns_real_req* real_req) {
    LOG("dns_real_req_start");
    struct timeval val= {5, 0};

    printf("realreq.active=%d,reqlreq.pending=%d", real_req->tmr_retry.active, real_req->tmr_retry.pending);

    ev_timer_init(&real_req->tmr_retry, dns_real_req_timercb, 5, 0);
    ev_timer_start(real_req->base->loop, &real_req->tmr_retry);
    real_req->tmr_retry.data = real_req;
    
    //evtimer_add(real_req->tmr_retry, &val);
    dns_send_req(real_req->base, real_req->host);
}
static void dns_real_req_cancel_free(struct dns_real_req* req) {
    LOG("dns_real_req_cancel_free");
    struct ngdns_req* pos;
    struct ngdns_req* n;
    list_for_each_entry_safe(pos, n, &req->list_reqs, list) {
        list_del(&pos->list);
        free(pos);
    }

    // delete from hash table
    list_del(&req->list);
    free(req);
}

static void dns_real_req_active_and_free(struct ngdns_base* base, 
                                         struct dns_real_req* req, 
                                         struct response_record* resps,
                                         int resp_count,
                                         int ret_code) {
    LOG("dns_real_req_active_and_free");
    printf("dns_real_req_active_and_free\n");
    int i = 0;
    struct ngdns_req* pos = NULL;
    struct ngdns_req* n = NULL;
    const char * ip = NULL;
    int ip_count = 0;
    const char* cname = NULL;
    int64_t ttl = 10000;

    list_del(&req->list);

    if(resp_count > 0) {
        for(i = 0; i < resp_count; i++) {
            if(resps[i].type == 1) {
                ip = resps[i].ip;
                ttl = resps[i].ttl;
                break;
            } else if(resps[i].type == 5) {
                cname = resps[i].cname;
            }
        }
    }

    if(!cname) {
        cname = "";
    }

    if(!ip) {
      ret_code = -1;
    }

    if(resp_count == 0 & ret_code == 0) {
        ret_code = -1;
    }

    ev_timer_stop(base->loop, &req->tmr_retry);
    
    if(ret_code == 0 && ip) {
       // fprintf(stderr, "adding cache,host=%s, cname=%s, ip=%s, ttl=%lld\n", req->host, cname, ip, ttl);
        dns_cache_add(base, req->host, ip, cname, ttl);
    }

    list_for_each_entry_safe(pos, n, &req->list_reqs, list) {
        list_del(&pos->list);
        if(pos->cb) {
            pos->cb(req->host, ret_code, ip, cname, pos->args);
        }
        
        free(pos);
    }

    // delete from hash table
    
    free(req);
}

static int make_socket_nonblocking(int fd) {
  int flags;
  int rv;
  while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR)
    ;
  while ((rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR)
    ;
  return rv;
}

struct ngdns_base* ngdns_base_new(struct ev_loop* loop, const char* ns_addr) {
    LOG("ngdns_base_new");
    if(ns_addr == NULL || *ns_addr == '\0') {
        printf("use default dns 114");
        ns_addr = "114.114.114.114";
    }

    struct ngdns_base* base = (struct ngdns_base*)calloc(1, sizeof(struct ngdns_base));
    int i = 0;
    base->loop = loop;
    base->s_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    make_socket_nonblocking(base->s_fd);
    strncpy(base->s_ns_addr, ns_addr, sizeof(base->s_ns_addr));
    ev_io_init(&base->s_event, dns_recv_cb, base->s_fd, EV_READ);

    base->s_event.data = base;
    ev_io_start(base->loop, &base->s_event);

    base->s_server_addr.sin_family = AF_INET;
    base->s_server_addr.sin_port = htons(53);
    base->s_server_addr.sin_addr.s_addr = inet_addr(ns_addr);

    INIT_LIST_HEAD(&(base->list_head_pending_items));
 
    for(i = 0; i < HASH_BUCKET_SIZE; i++) {
        INIT_LIST_HEAD(&base->s_hash[i]);
    }

    for(i = 0; i < CACHE_HASH_BUCKET_SIZE; i++) {
        INIT_LIST_HEAD(&base->s_cache_hash[i]);
    }
    return base;
}

struct ngdns_base* ngdns_addr_reset(struct ngdns_base* base, const char* ns_addr) {
    base->s_server_addr.sin_addr.s_addr = inet_addr(ns_addr);
}

struct ngdns_req* ngdns_resolve(struct ngdns_base* base, const char* host, ngdns_cb cb, void* args) {
    LOG("ngdns_resolve");
    struct ngdns_req* req= (struct ngdns_req*)calloc(1, sizeof(struct ngdns_req));
    req->cb = cb;
    req->args = args;
    req->base = base;
    printf("resolve start\n");
    struct in_addr inaddr;
    strcpy(req->host, host);
    if (inet_aton(host, &inaddr)) {
        printf("ip found\n");
        strcpy(req->cname, "fake.cname");
        strcpy(req->ip, host);
        req->tmr_activate.data = req;
        list_add(&(req->list_pending_item), &(base->list_head_pending_items));
        ev_timer_init(&req->tmr_activate, dns_req_activatecb, 0, 0);
        ev_timer_start(base->loop, &req->tmr_activate);
        return req;
        // TODO: return a cached item        
        //return inet_ntoa(inaddr);
    }

    struct cache_record* record = dns_cache_find(base, host);

    if(record) {
        fprintf(stderr, "host %s cached\n", host);
        strcpy(req->cname, record->cname);
        strcpy(req->ip, record->ip);
        req->tmr_activate.data = req;
        list_add(&(req->list_pending_item), &(base->list_head_pending_items));
        ev_timer_init(&req->tmr_activate, dns_req_activatecb, 0, 0);
        ev_timer_start(base->loop, &req->tmr_activate);
        return req;
    }

    printf("cache not found\n");
    struct dns_real_req* real_req = dns_real_req_find(base, host);
    if(!real_req) {
        real_req = (struct dns_real_req*)calloc(1, sizeof(struct dns_real_req));
        real_req->base = base;
       
        // TODO:fix retry timer
        //real_req->tmr_retry = evtimer_new(s_base->base, dns_real_req_timercb, real_req);

        INIT_LIST_HEAD(&real_req->list_reqs);
        strncpy(real_req->host, host, sizeof(real_req->host));
        dns_real_req_add(real_req);
        dns_real_req_start(real_req);
    }

    req->real_req = real_req;

    list_add(&(req->list), &(real_req->list_reqs));
    return req;
}


static unsigned char *install_domain_name(unsigned char *p, const char *domain_name)
{
    LOG("install_domain_name");
    // .lemuria.cis.vtc.edu\0
    *p++ = '.';
    strcpy((char *)p, domain_name);
    p--;

    while (*p != '\0') {
        if (*p == '.') {
            unsigned char *end = p + 1;
            while (*end != '.' && *end != '\0') end++;
            *p = end - p - 1;
        }
        p++;
    }
    return p + 1;
}

static unsigned char* decode_enc_str(unsigned char* msg_begin, unsigned char* p, char* buf) {
    while(*p) {
        int len = *p;

        if(len == 0xc0) {
            int offset = (*p & 0x3f) * 255 + (*(p + 1));
            decode_enc_str(msg_begin, msg_begin + offset, buf);
            p += 2;
            return p;
        } else {
            p ++;
            memcpy(buf + strlen(buf), p, len);
            strcat(buf, ".");
            p += len;
        }
    }
    buf[strlen(buf) - 1] = 0;
    p ++;
    return p;
}

static unsigned char* decode_request_items(unsigned char* msg_begin, unsigned char* p, struct request_record* rec) {
    LOG("decode_request_items");
    p = decode_enc_str(msg_begin, p, rec->host);
    printf("host = %s\n", rec->host);
    p += 4;
    return p;
}



static unsigned char* decode_resopnse_item(unsigned char* msg_begin, unsigned char* p, struct response_record* resp) {
    LOG("decode_resopnse_item");
    int i = 0;
    p = decode_enc_str(msg_begin, p, resp->host);
    
    int type = (*p) * 255 + (*(p + 1));
    p += 2;
    int qclass = (*p) * 255 + (*(p + 1));
    p += 2;
    int ttl = ntohl(*(uint32_t*)p);

    p += 4;
    int rdlen = (*p) * 255 + (*(p + 1));
    p += 2;

    resp->type = type;
    
    if(type == 0x1) {
        for(i = 0; i < 4; i++) {
            printf("origin tll=%d\n", ttl);
            snprintf(resp->ip, sizeof(resp->ip), "%d.%d.%d.%d", *p, *(p+1), *(p+2), *(p+3));
            resp->ttl = ttl;
        }
        //printf("ip:%s\n", resp->ip);
    } else if(type == 0x5) {
        decode_enc_str(msg_begin, p, resp->cname);
    printf("cname:%s\n", resp->cname);
    }
    
    p += rdlen;

    return p;

}

static void decode_response(struct ngdns_base* base, const char* resp, int len) {
    LOG("decode_response");   
    int i = 0;
    struct request_record* reqs = NULL;
    struct response_record* resps = NULL;
    unsigned char* p = (unsigned char*)resp;
    int id = (*p) * 255 + (*(p + 1));
    int qr = (*(p + 2)) >> 7;
    int rcode = (*(p + 3) & 0xf);
    int dcount = (*(p + 4)) * 255 + (*(p + 5));
    int answer_count = (*(p + 6)) * 255 + (*(p + 7));



    if(dcount) {
        reqs = (struct request_record*)calloc(1, sizeof(struct request_record) * dcount);
    }

    if(answer_count) {
        resps = (struct response_record*)calloc(1, sizeof(struct response_record) * answer_count);
    }
    
    // skip header
    p += 12;

    // skip request
    for(i = 0; i< dcount; i++) {
        p = decode_request_items((unsigned char*)resp, p, reqs + i);
    }

    for(i = 0; i < answer_count; i++) {
        p = decode_resopnse_item((unsigned char*)resp, p, resps + i);
    }

    if(reqs) {
        const char* host = reqs[0].host;
        len = strlen(host);
       // if(host && strlen(host > 0)) {
        if(len) {
             struct dns_real_req* real_req = dns_real_req_find(base, host);

            fprintf(stderr, "request %s rcode=%d\n",  host, rcode);

            if(real_req) {
                dns_real_req_active_and_free(base, real_req, resps, answer_count, rcode);
            } 
        }
           
    //    }
               
    }
    
    if(reqs) {
        free(reqs);
    }

    if(resps) {
        free(resps);
    }
}

static int dns_send_req(struct ngdns_base* base, const char* host)
{
    LOG("dns_send_req");   
    printf("sending req\n");
    unsigned char  send_buf[WORKSPACE_SIZE] = {};
    
    unsigned char *p = NULL;
    int rc;
    int n = random();
    char* p1 = (char*)&n;

    p = send_buf;
    p[0] = p1[0];
    p[1] = p1[1];
    //p[1] = 0x1;
    p[2] = 0x1; //QR = 0, Opcode = 0, AA = 0, TC = 0, RD = 1.
    p[5] = 0x1; //QDCOUNT = 1
    p += 12;

     p = install_domain_name(p, host);
     p[1] = 0x1; // qtype=1
     p[3] = 1;
     p += 4;


    rc = sendto(base->s_fd, send_buf, p - send_buf, 0, (struct sockaddr *)&base->s_server_addr, sizeof(struct sockaddr_in));
    if(rc >= 0) {
        return 0;
    } else {
        return -1;
    }
}

void dns_recv_cb(struct ev_loop *loop, ev_io *w, int revents) {
    LOG("dns_recv_cb");   
    struct ngdns_base* base = (struct ngdns_base*)w->data;
    ev_io_start(loop, &base->s_event);
    struct sockaddr_in server_addr = {};
    socklen_t addr_len = sizeof(struct sockaddr_in);
    

    if(revents != EV_READ) {
        return;
    }
    static char recv_buf[1024] = {};
    int rc = recvfrom(base->s_fd, recv_buf, 1024, 0, (struct sockaddr *)&server_addr, &addr_len);
    if( rc == -1 ) {
        return;
    }

    decode_response(base, recv_buf, rc);
}

void ngdns_base_free(struct ngdns_base* base) {
    LOG("ngdns_base_free");   
    struct cache_record* pos = NULL;
    struct cache_record* tmp = NULL;

    struct ngdns_req* pos_req = NULL;
    struct ngdns_req* tmp_req = NULL;

    struct dns_real_req* pos_real_req = NULL;
    struct dns_real_req* tmp_real_req = NULL;
    int i = 0;
    for(; i < CACHE_HASH_BUCKET_SIZE; i++) {
        list_for_each_entry_safe(pos, tmp, &base->s_cache_hash[i], list) {
            list_del(&pos->list);
            free(pos);
        }
    }

    ev_io_stop(base->loop, &base->s_event);
    close(base->s_fd);
    free(base);
}
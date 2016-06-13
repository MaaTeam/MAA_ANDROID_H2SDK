/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "shrpx_client_handler.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H
#include <cerrno>

#include "shrpx_upstream.h"
#include "shrpx_http2_upstream.h"
#include "shrpx_https_upstream.h"
#include "shrpx_config.h"
#include "shrpx_http_downstream_connection.h"
#include "shrpx_http2_downstream_connection.h"
#include "shrpx_ssl.h"
#include "shrpx_worker.h"
#include "shrpx_downstream_connection_pool.h"
#include "shrpx_downstream.h"
#include "shrpx_http2_session.h"
#ifdef HAVE_SPDYLAY
#include "shrpx_spdy_upstream.h"
#endif // HAVE_SPDYLAY
#include "util.h"
#include "template.h"
#include "ssl.h"

using namespace nghttp2;
static list_head_t s_list_conns = LIST_HEAD_INIT(s_list_conns);
namespace shrpx {

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto handler = static_cast<ClientHandler *>(conn->data);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, handler) << "Time out";
  }

  delete handler;
}
} // namespace

namespace {
void shutdowncb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto handler = static_cast<ClientHandler *>(w->data);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, handler) << "Close connection due to TLS renegotiation";
  }

  delete handler;
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto handler = static_cast<ClientHandler *>(conn->data);

  if (handler->do_read() != 0) {
    delete handler;
    return;
  }
  if (handler->do_write() != 0) {
    delete handler;
    return;
  }
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto handler = static_cast<ClientHandler *>(conn->data);

  if (handler->do_write() != 0) {
    delete handler;
    return;
  }
}
} // namespace

int ClientHandler::noop() { return 0; }

int ClientHandler::read_clear() {
  ev_timer_again(conn_.loop, &conn_.rt);

  for (;;) {
    if (rb_.rleft() && on_read() != 0) {
      return -1;
    }
    if (rb_.rleft() == 0) {
      rb_.reset();
    } else if (rb_.wleft() == 0) {
      conn_.rlimit.stopw();
      return 0;
    }

    auto nread = conn_.read_clear(rb_.last, rb_.wleft());

    if (nread == 0) {
      return 0;
    }

    if (nread < 0) {
      return -1;
    }

    rb_.write(nread);
  }
}

int ClientHandler::write_clear() {
  std::array<iovec, 2> iov;

  ev_timer_again(conn_.loop, &conn_.rt);

  for (;;) {
    if (on_write() != 0) {
      return -1;
    }

    auto iovcnt = upstream_->response_riovec(iov.data(), iov.size());
    if (iovcnt == 0) {
      break;
    }

    auto nwrite = conn_.writev_clear(iov.data(), iovcnt);
    if (nwrite < 0) {
      return -1;
    }

    if (nwrite == 0) {
      return 0;
    }

    upstream_->response_drain(nwrite);
  }

  conn_.wlimit.stopw();
  ev_timer_stop(conn_.loop, &conn_.wt);

  return 0;
}

int ClientHandler::tls_handshake() {
  ev_timer_again(conn_.loop, &conn_.rt);

  ERR_clear_error();

  auto rv = conn_.tls_handshake();

  if (rv == SHRPX_ERR_INPROGRESS) {
    return 0;
  }

  if (rv < 0) {
    return -1;
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "SSL/TLS handshake completed";
  }

  if (validate_next_proto() != 0) {
    return -1;
  }

  read_ = &ClientHandler::read_tls;
  write_ = &ClientHandler::write_tls;

  return 0;
}

int ClientHandler::read_tls() {
  ev_timer_again(conn_.loop, &conn_.rt);

  ERR_clear_error();

  for (;;) {
    // we should process buffered data first before we read EOF.
    if (rb_.rleft() && on_read() != 0) {
      return -1;
    }
    if (rb_.rleft() == 0) {
      rb_.reset();
    } else if (rb_.wleft() == 0) {
      conn_.rlimit.stopw();
      return 0;
    }

    auto nread = conn_.read_tls(rb_.last, rb_.wleft());

    if (nread == 0) {
      return 0;
    }

    if (nread < 0) {
      return -1;
    }

    rb_.write(nread);
  }
}

int ClientHandler::write_tls() {
  struct iovec iov;

  ev_timer_again(conn_.loop, &conn_.rt);

  ERR_clear_error();

  for (;;) {
    if (on_write() != 0) {
      return -1;
    }

    auto iovcnt = upstream_->response_riovec(&iov, 1);
    if (iovcnt == 0) {
      conn_.start_tls_write_idle();
      break;
    }

    auto nwrite = conn_.write_tls(iov.iov_base, iov.iov_len);
    if (nwrite < 0) {
      return -1;
    }

    if (nwrite == 0) {
      return 0;
    }

    upstream_->response_drain(nwrite);
  }

  conn_.wlimit.stopw();
  ev_timer_stop(conn_.loop, &conn_.wt);

  return 0;
}

int ClientHandler::upstream_noop() { return 0; }

int ClientHandler::upstream_read() {
  assert(upstream_);
  if (upstream_->on_read() != 0) {
    return -1;
  }
  return 0;
}

int ClientHandler::upstream_write() {
  assert(upstream_);
  if (upstream_->on_write() != 0) {
    return -1;
  }

  if (get_should_close_after_write() && upstream_->response_empty()) {
    return -1;
  }

  return 0;
}

int ClientHandler::upstream_http2_connhd_read() {
  auto nread = std::min(left_connhd_len_, rb_.rleft());
  if (memcmp(NGHTTP2_CLIENT_MAGIC + NGHTTP2_CLIENT_MAGIC_LEN - left_connhd_len_,
             rb_.pos, nread) != 0) {
    // There is no downgrade path here. Just drop the connection.
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "invalid client connection header";
    }

    return -1;
  }

  left_connhd_len_ -= nread;
  rb_.drain(nread);
  conn_.rlimit.startw();

  if (left_connhd_len_ == 0) {
    on_read_ = &ClientHandler::upstream_read;
    // Run on_read to process data left in buffer since they are not
    // notified further
    if (on_read() != 0) {
      return -1;
    }
    return 0;
  }

  return 0;
}

int ClientHandler::upstream_http1_connhd_read() {
  auto nread = std::min(left_connhd_len_, rb_.rleft());
  if (memcmp(NGHTTP2_CLIENT_MAGIC + NGHTTP2_CLIENT_MAGIC_LEN - left_connhd_len_,
             rb_.pos, nread) != 0) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "This is HTTP/1.1 connection, "
                       << "but may be upgraded to HTTP/2 later." 
                       << "getTid: ";
    }

    // Reset header length for later HTTP/2 upgrade
    left_connhd_len_ = NGHTTP2_CLIENT_MAGIC_LEN;
    on_read_ = &ClientHandler::upstream_read;
    on_write_ = &ClientHandler::upstream_write;

    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }

  left_connhd_len_ -= nread;
  rb_.drain(nread);
  conn_.rlimit.startw();

  if (left_connhd_len_ == 0) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "direct HTTP/2 connection";
    }

    direct_http2_upgrade();
    on_read_ = &ClientHandler::upstream_read;
    on_write_ = &ClientHandler::upstream_write;

    // Run on_read to process data left in buffer since they are not
    // notified further
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }

  return 0;
}

ClientHandler::ClientHandler(Worker *worker, int fd, SSL *ssl,
                             const char *ipaddr, const char *port)
    : conn_(worker->get_loop(), fd, ssl, worker->get_mcpool(),
            get_config()->upstream_write_timeout,
            get_config()->upstream_read_timeout, get_config()->write_rate,
            get_config()->write_burst, get_config()->read_rate,
            get_config()->read_burst, writecb, readcb, timeoutcb, this,
            get_config()->tls_dyn_rec_warmup_threshold,
            get_config()->tls_dyn_rec_idle_timeout),
      pinned_http2sessions_(
          get_config()->downstream_proto == PROTO_HTTP2
              ? make_unique<std::vector<ssize_t>>(
                    get_config()->downstream_addr_groups.size(), -1)
              : nullptr),
      ipaddr_(ipaddr), port_(port), worker_(worker),
      left_connhd_len_(NGHTTP2_CLIENT_MAGIC_LEN),
      dns_req_(NULL),
      should_close_after_write_(false) {

  list_add(&(this->list), &s_list_conns);

  ++worker_->get_worker_stat()->num_connections;

  ev_timer_init(&reneg_shutdown_timer_, shutdowncb, 0., 0.);

  reneg_shutdown_timer_.data = this;

  conn_.rlimit.startw();
  ev_timer_again(conn_.loop, &conn_.rt);

  if (get_config()->accept_proxy_protocol) {
    read_ = &ClientHandler::read_clear;
    write_ = &ClientHandler::noop;
    on_read_ = &ClientHandler::proxy_protocol_read;
    on_write_ = &ClientHandler::upstream_noop;
  } else {
    setup_upstream_io_callback();
  }
}

void ClientHandler::disconnect_all() {
  CLOG(INFO, NULL) << "disconnect all called" ;
  list_head_t* pos;
  list_head_t* tmp;
  
  list_for_each_safe(pos, tmp, &s_list_conns) {
     CLOG(INFO, pos) << "disconnecting client handler" ;
    int offset = offsetof(ClientHandler, list);
    char* char_pos = (char*)pos;
    ClientHandler* handler = (ClientHandler*)(char_pos - offset);
    delete handler;
  }
}

void ClientHandler::setup_upstream_io_callback() {
  if (conn_.tls.ssl) {
    conn_.prepare_server_handshake();
    read_ = write_ = &ClientHandler::tls_handshake;
    on_read_ = &ClientHandler::upstream_noop;
    on_write_ = &ClientHandler::upstream_write;
  } else {
    // For non-TLS version, first create HttpsUpstream. It may be
    // upgraded to HTTP/2 through HTTP Upgrade or direct HTTP/2
    // connection.
    CLOG(INFO, this) << "setup_upstream_io_callback";
    upstream_ = make_unique<HttpsUpstream>(this);
    alpn_ = "http/1.1";
    read_ = &ClientHandler::read_clear;
    write_ = &ClientHandler::write_clear;
    on_read_ = &ClientHandler::upstream_http1_connhd_read;
    on_write_ = &ClientHandler::upstream_noop;
  }
}

ClientHandler::~ClientHandler() {
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Deleting";
  }
  list_del(&this->list);
  if (upstream_) {
    upstream_->on_handler_delete();
  }

  auto worker_stat = worker_->get_worker_stat();
  --worker_stat->num_connections;

  if (worker_stat->num_connections == 0) {
    worker_->schedule_clear_mcpool();
  }

  ev_timer_stop(conn_.loop, &reneg_shutdown_timer_);

  // TODO If backend is http/2, and it is in CONNECTED state, signal
  // it and make it loopbreak when output is zero.
  if (worker_->get_graceful_shutdown() && worker_stat->num_connections == 0) {
    ev_break(conn_.loop);
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Deleted";
  }
}

Upstream *ClientHandler::get_upstream() { return upstream_.get(); }

struct ev_loop *ClientHandler::get_loop() const {
  return conn_.loop;
}

void ClientHandler::reset_upstream_read_timeout(ev_tstamp t) {
  conn_.rt.repeat = t;
  if (ev_is_active(&conn_.rt)) {
    ev_timer_again(conn_.loop, &conn_.rt);
  }
}

void ClientHandler::reset_upstream_write_timeout(ev_tstamp t) {
  conn_.wt.repeat = t;
  if (ev_is_active(&conn_.wt)) {
    ev_timer_again(conn_.loop, &conn_.wt);
  }
}

int ClientHandler::validate_next_proto() {
  const unsigned char *next_proto = nullptr;
  unsigned int next_proto_len;

  // First set callback for catch all cases
  on_read_ = &ClientHandler::upstream_read;

  SSL_get0_next_proto_negotiated(conn_.tls.ssl, &next_proto, &next_proto_len);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
  if (next_proto == nullptr) {
    SSL_get0_alpn_selected(conn_.tls.ssl, &next_proto, &next_proto_len);
  }
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L

  if (next_proto == nullptr) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "No protocol negotiated. Fallback to HTTP/1.1";
    }

    upstream_ = make_unique<HttpsUpstream>(this);
    alpn_ = "http/1.1";

    // At this point, input buffer is already filled with some bytes.
    // The read callback is not called until new data come. So consume
    // input buffer here.
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }

  if (LOG_ENABLED(INFO)) {
    std::string proto(next_proto, next_proto + next_proto_len);
    CLOG(INFO, this) << "The negotiated next protocol: " << proto;
  }

  if (!ssl::in_proto_list(get_config()->npn_list, next_proto, next_proto_len)) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "The negotiated protocol is not supported";
    }
    return -1;
  }

  if (util::check_h2_is_selected(next_proto, next_proto_len)) {
    on_read_ = &ClientHandler::upstream_http2_connhd_read;

    auto http2_upstream = make_unique<Http2Upstream>(this);

    upstream_ = std::move(http2_upstream);
    alpn_.assign(next_proto, next_proto + next_proto_len);

    // At this point, input buffer is already filled with some bytes.
    // The read callback is not called until new data come. So consume
    // input buffer here.
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }

#ifdef HAVE_SPDYLAY
  auto spdy_version = spdylay_npn_get_version(next_proto, next_proto_len);
  if (spdy_version) {
    upstream_ = make_unique<SpdyUpstream>(spdy_version, this);

    switch (spdy_version) {
    case SPDYLAY_PROTO_SPDY2:
      alpn_ = "spdy/2";
      break;
    case SPDYLAY_PROTO_SPDY3:
      alpn_ = "spdy/3";
      break;
    case SPDYLAY_PROTO_SPDY3_1:
      alpn_ = "spdy/3.1";
      break;
    default:
      alpn_ = "spdy/unknown";
    }

    // At this point, input buffer is already filled with some bytes.
    // The read callback is not called until new data come. So consume
    // input buffer here.
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }
#endif // HAVE_SPDYLAY

  if (next_proto_len == 8 && memcmp("http/1.1", next_proto, 8) == 0) {
    upstream_ = make_unique<HttpsUpstream>(this);
    alpn_ = "http/1.1";

    // At this point, input buffer is already filled with some bytes.
    // The read callback is not called until new data come. So consume
    // input buffer here.
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "The negotiated protocol is not supported";
  }
  return -1;
}

int ClientHandler::do_read() { return read_(*this); }
int ClientHandler::do_write() { return write_(*this); }

int ClientHandler::on_read() {
  auto rv = on_read_(*this);
  if (rv != 0) {
    return rv;
  }
  conn_.handle_tls_pending_read();
  return 0;
}
int ClientHandler::on_write() { return on_write_(*this); }

const std::string &ClientHandler::get_ipaddr() const { return ipaddr_; }

bool ClientHandler::get_should_close_after_write() const {
  return should_close_after_write_;
}

void ClientHandler::set_should_close_after_write(bool f) {
  should_close_after_write_ = f;
}

void ClientHandler::pool_downstream_connection(
    std::unique_ptr<DownstreamConnection> dconn) {
  if (!dconn->poolable()) {
    return;
  }
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Pooling downstream connection DCONN:" << dconn.get()
                     << " in group " << dconn->get_group();
  }
  dconn->set_client_handler(nullptr);
  auto dconn_pool = worker_->get_dconn_pool();
  dconn_pool->add_downstream_connection(std::move(dconn));
}

void ClientHandler::remove_downstream_connection(DownstreamConnection *dconn) {
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Removing downstream connection DCONN:" << dconn
                     << " from pool";
  }
  auto dconn_pool = worker_->get_dconn_pool();
  dconn_pool->remove_downstream_connection(dconn);
}

std::unique_ptr<DownstreamConnection>
ClientHandler::new_downstream_connection(size_t group, bool isH2) {
  auto dconn_pool = worker_->get_dconn_pool();
  auto dconn = dconn_pool->pop_downstream_connection(group);

  if(!dconn)
    if(isH2) {
      CLOG(INFO, this) << "downstream_proto == PROTO_HTTP2: " ;
      int span = group - pinned_http2sessions_->size() + 1;
      while (span > 0) {
        span -- ;
        pinned_http2sessions_->push_back(-1);
      }

      Http2Session *http2session;
      ssize_t &pinned = (*pinned_http2sessions_)[group];
      if (pinned == -1) {
        http2session = worker_->next_http2_session(group);
        pinned = http2session->get_index();
        CLOG(INFO, this) << "group:" << group << " :"<< pinned;
      } else {
        auto dgrp = worker_->get_dgrp(group);
        http2session = dgrp->http2sessions[pinned].get();
      }
      dconn = make_unique<Http2DownstreamConnection>(dconn_pool, http2session);
    } else {
      CLOG(INFO, this) << "downstream_proto != PROTO_HTTP2: " ;
      dconn =
        make_unique<HttpDownstreamConnection>(dconn_pool, group, conn_.loop);
    }

  dconn->set_client_handler(this);
  return dconn;
}

size_t ClientHandler::add_downstream_group(std::string pattern, DownstreamAddr& addr) {
  CLOG(INFO, this) << "new downstream group pattern: " << pattern;
  int group = -1;
  DownstreamAddrGroup g(pattern + "/");
  g.addrs.push_back(addr);
  mod_config()->router.add_route(g.pattern.get(), pattern.length(),
                               get_config()->downstream_addr_groups.size());
  mod_config()->downstream_addr_groups.push_back(std::move(g));
  
  group = get_config()->downstream_addr_groups.size() -1;

  worker_->add_new_group();

  //pinned_http2sessions_->resize(get_config()->downstream_addr_groups.size()); // fix me
  int session = get_config()->downstream_addr_groups.size() - pinned_http2sessions_->size();
  for(;session > 0; ) {
    session--;
    pinned_http2sessions_->push_back(-1);
  }

  CLOG(INFO, this) << "----------------------";
  CLOG(INFO, this) << "get_config()->downstream_addr_groups.size: " << get_config()->downstream_addr_groups.size();
  CLOG(INFO, this) << "pinned_http2sessions_.size: " << pinned_http2sessions_->size();
  CLOG(INFO, this) << "group: " << group;

  return group;
}

size_t ClientHandler::get_downstream_group(Downstream *downstream) {
  size_t group ;
  auto &groups = get_config()->downstream_addr_groups;
  auto catch_all = get_config()->downstream_addr_group_catch_all;

  // Fast path.  If we have one group, it must be catch-all group.
  // HTTP/2 and client proxy modes fall in this case.
  /*if (groups.size() == 1) {
    group = 0;
  } else*///if (downstream->get_request_method() == HTTP_CONNECT) {
    //  We don't know how to treat CONNECT request in host-path
    //  mapping.  It most likely appears in proxy scenario.  Since we
    //  have dealt with proxy case already, just use catch-all group.
   // group = catch_all;
 // } else {
    std::stringstream hostport;
    int32_t port = 0;
    if (!downstream->get_request_http2_authority().empty()) {
      if (downstream->access_info_._relViaProxy &&
            downstream->access_info_._is_protocol_h2) {
        // we may rematch this after dns resolve
        port = 6480;
        hostport << downstream->get_request_http2_authority() << ":" << port;
      }
      else {
        std::string authority = downstream->get_request_http2_authority();
        auto p = std::find(std::begin(authority), std::end(authority), ':');
        if (p == std::end(authority) ) {
          port = 80;   // fix me
          hostport << downstream->get_request_http2_authority() << ":" << port;
        } else {
          hostport << downstream->get_request_http2_authority();
        }
      }
    }

    auto &router = get_config()->router;
    if (!downstream->get_request_http2_authority().empty()) {
        CLOG(INFO, this) << "match group, use pattern: " << hostport.str();
        group = match_downstream_addr_group(
          router, hostport.str(),
          downstream->get_request_path(), groups, catch_all);
    } else {
      CLOG(INFO, this) << " get_request_http2_authority().empty()";
      auto h = downstream->get_request_header(http2::HD_HOST);
      if (h) {
        group = match_downstream_addr_group(router, h->value,
                                            downstream->get_request_path(),
                                            groups, catch_all);
      } else {
        group = match_downstream_addr_group(
            router, "", downstream->get_request_path(), groups, catch_all);
      }
    }
 // }

  CLOG(INFO, this) << "Pattern is: " << hostport.str() << "\tgroup: " << group;

  return group;
}

std::unique_ptr<DownstreamConnection>
ClientHandler::get_downstream_connection(size_t group) {
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Downstream address group: " << group;
  }

  auto dconn_pool = worker_->get_dconn_pool();
  auto dconn = dconn_pool->pop_downstream_connection(group);

  if(dconn)
    dconn->set_client_handler(this);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Reuse downstream connection DCONN:" << dconn.get()
                     << " from pool group" << group;
  }

  return dconn;
}

MemchunkPool *ClientHandler::get_mcpool() { return worker_->get_mcpool(); }

SSL *ClientHandler::get_ssl() const { return conn_.tls.ssl; }

ConnectBlocker *ClientHandler::get_connect_blocker() const {
  CLOG(INFO, this) << "client handler get_connect_blocker worker_: " <<&worker_;
  return worker_->get_connect_blocker();
}

void ClientHandler::direct_http2_upgrade() {
  upstream_ = make_unique<Http2Upstream>(this);
  alpn_ = NGHTTP2_CLEARTEXT_PROTO_VERSION_ID;
  on_read_ = &ClientHandler::upstream_read;
  write_ = &ClientHandler::write_clear;
}

int ClientHandler::perform_http2_upgrade(HttpsUpstream *http) {
  auto upstream = make_unique<Http2Upstream>(this);

  auto output = upstream->get_response_buf();

  // We might have written non-final header in response_buf, in this
  // case, response_state is still INITIAL.  If this non-final header
  // and upgrade header fit in output buffer, do upgrade.  Otherwise,
  // to avoid to send this non-final header as response body in HTTP/2
  // upstream, fail upgrade.
  auto downstream = http->get_downstream();
  auto input = downstream->get_response_buf();

  static constexpr char res[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Connection: Upgrade\r\n"
      "Upgrade: " NGHTTP2_CLEARTEXT_PROTO_VERSION_ID "\r\n"
      "\r\n";

  auto required_size = str_size(res) + input->rleft();

  if (output->wleft() < required_size) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this)
          << "HTTP Upgrade failed because of insufficient buffer space: need "
          << required_size << ", available " << output->wleft();
    }
    return -1;
  }

  if (upstream->upgrade_upstream(http) != 0) {
    return -1;
  }
  // http pointer is now owned by upstream.
  upstream_.release();
  // TODO We might get other version id in HTTP2-settings, if we
  // support aliasing for h2, but we just use library default for now.
  alpn_ = NGHTTP2_CLEARTEXT_PROTO_VERSION_ID;
  on_read_ = &ClientHandler::upstream_http2_connhd_read;
  write_ = &ClientHandler::write_clear;

  auto nread =
      downstream->get_response_buf()->remove(output->last, output->wleft());
  output->write(nread);

  output->write(res, str_size(res));
  upstream_ = std::move(upstream);

  signal_write();
  return 0;
}

bool ClientHandler::get_http2_upgrade_allowed() const { return !conn_.tls.ssl; }

std::string ClientHandler::get_upstream_scheme() const {
  if (conn_.tls.ssl) {
    return "https";
  } else {
    return "http";
  }
}

void ClientHandler::start_immediate_shutdown() {
  ev_timer_start(conn_.loop, &reneg_shutdown_timer_);
}

namespace {
// Construct absolute request URI from |downstream|, mainly to log
// request URI for proxy request (HTTP/2 proxy or client proxy).  This
// is mostly same routine found in
// HttpDownstreamConnection::push_request_headers(), but vastly
// simplified since we only care about absolute URI.
std::string construct_absolute_request_uri(Downstream *downstream) {
  auto &authority = downstream->get_request_http2_authority();
  if (authority.empty()) {
    return downstream->get_request_path();
  }
  std::string uri;
  auto &scheme = downstream->get_request_http2_scheme();
  if (scheme.empty()) {
    // We may have to log the request which lacks scheme (e.g.,
    // http/1.1 with origin form).
    uri += "http://";
  } else {
    uri += scheme;
    uri += "://";
  }
  uri += authority;
  uri += downstream->get_request_path();

  return uri;
}
} // namespace

void ClientHandler::write_accesslog(std::string logline) {
  access_log(logline);
}

void ClientHandler::write_accesslog(Downstream *downstream) {
  nghttp2::ssl::TLSSessionInfo tls_info;

  upstream_accesslog(
      get_config()->accesslog_format,
      LogSpec{
          downstream, ipaddr_.c_str(),
          http2::to_method_string(downstream->get_request_method()),

          downstream->get_request_method() == HTTP_CONNECT
              ? downstream->get_request_http2_authority().c_str()
              : (get_config()->http2_proxy || get_config()->client_proxy)
                    ? construct_absolute_request_uri(downstream).c_str()
                    : downstream->get_request_path().empty()
                          ? downstream->get_request_method() == HTTP_OPTIONS
                                ? "*"
                                : "-"
                          : downstream->get_request_path().c_str(),

          alpn_.c_str(),
          nghttp2::ssl::get_tls_session_info(&tls_info, conn_.tls.ssl),

          std::chrono::system_clock::now(),          // time_now
          downstream->get_request_start_time(),      // request_start_time
          std::chrono::high_resolution_clock::now(), // request_end_time

          downstream->get_request_major(), downstream->get_request_minor(),
          downstream->get_response_http_status(),
          downstream->get_response_sent_bodylen(), port_.c_str(),
          get_config()->port, get_config()->pid,
      });
}

void ClientHandler::write_accesslog(int major, int minor, unsigned int status,
                                    int64_t body_bytes_sent) {
  auto time_now = std::chrono::system_clock::now();
  auto highres_now = std::chrono::high_resolution_clock::now();
  nghttp2::ssl::TLSSessionInfo tls_info;

  upstream_accesslog(get_config()->accesslog_format,
                     LogSpec{
                         nullptr, ipaddr_.c_str(),
                         "-", // method
                         "-", // path,
                         alpn_.c_str(), nghttp2::ssl::get_tls_session_info(
                                            &tls_info, conn_.tls.ssl),
                         time_now,
                         highres_now,  // request_start_time TODO is
                                       // there a better value?
                         highres_now,  // request_end_time
                         major, minor, // major, minor
                         status, body_bytes_sent, port_.c_str(),
                         get_config()->port, get_config()->pid,
                     });
}

ClientHandler::ReadBuf *ClientHandler::get_rb() { return &rb_; }

void ClientHandler::signal_write() { conn_.wlimit.startw(); }

RateLimit *ClientHandler::get_rlimit() { return &conn_.rlimit; }
RateLimit *ClientHandler::get_wlimit() { return &conn_.wlimit; }

ev_io *ClientHandler::get_wev() { return &conn_.wev; }

Worker *ClientHandler::get_worker() const { return worker_; }


namespace {
ssize_t parse_proxy_line_port(const uint8_t *first, const uint8_t *last) {
  auto p = first;
  int32_t port = 0;

  if (p == last) {
    return -1;
  }

  if (*p == '0') {
    if (p + 1 != last && util::is_digit(*(p + 1))) {
      return -1;
    }
    return 1;
  }

  for (; p != last && util::is_digit(*p); ++p) {
    port *= 10;
    port += *p - '0';

    if (port > 65535) {
      return -1;
    }
  }

  return p - first;
}
} // namespace

int ClientHandler::on_proxy_protocol_finish() {
  if (conn_.tls.ssl) {
    conn_.tls.rbuf.append(rb_.pos, rb_.rleft());
    rb_.reset();
  }

  setup_upstream_io_callback();

  // Run on_read to process data left in buffer since they are not
  // notified further
  if (on_read() != 0) {
    return -1;
  }

  return 0;
}

// http://www.haproxy.org/download/1.5/doc/proxy-protocol.txt
int ClientHandler::proxy_protocol_read() {
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol: Started";
  }

  auto first = rb_.pos;

  // NULL character really destroys functions which expects NULL
  // terminated string.  We won't expect it in PROXY protocol line, so
  // find it here.
  auto chrs = std::array<char, 2>{{'\n', '\0'}};

  constexpr size_t MAX_PROXY_LINELEN = 107;

  auto bufend = rb_.pos + std::min(MAX_PROXY_LINELEN, rb_.rleft());

  auto end =
      std::find_first_of(rb_.pos, bufend, std::begin(chrs), std::end(chrs));

  if (end == bufend || *end == '\0' || end == rb_.pos || *(end - 1) != '\r') {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: No ending CR LF sequence found";
    }
    return -1;
  }

  --end;

  constexpr const char HEADER[] = "PROXY ";

  if (static_cast<size_t>(end - rb_.pos) < str_size(HEADER)) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: PROXY version 1 ID not found";
    }
    return -1;
  }

  if (!util::streq_l(HEADER, rb_.pos, str_size(HEADER))) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Bad PROXY protocol version 1 ID";
    }
    return -1;
  }

  rb_.drain(str_size(HEADER));

  int family;

  if (rb_.pos[0] == 'T') {
    if (end - rb_.pos < 5) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: INET protocol family not found";
      }
      return -1;
    }

    if (rb_.pos[1] != 'C' || rb_.pos[2] != 'P') {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: Unknown INET protocol family";
      }
      return -1;
    }

    switch (rb_.pos[3]) {
    case '4':
      family = AF_INET;
      break;
    case '6':
      family = AF_INET6;
      break;
    default:
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: Unknown INET protocol family";
      }
      return -1;
    }

    rb_.drain(5);
  } else {
    if (end - rb_.pos < 7) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: INET protocol family not found";
      }
      return -1;
    }
    if (!util::streq_l("UNKNOWN", rb_.pos, 7)) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: Unknown INET protocol family";
      }
      return -1;
    }

    rb_.drain(end + 2 - rb_.pos);

    return on_proxy_protocol_finish();
  }

  // source address
  auto token_end = std::find(rb_.pos, end, ' ');
  if (token_end == end) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Source address not found";
    }
    return -1;
  }

  *token_end = '\0';
  if (!util::numeric_host(reinterpret_cast<const char *>(rb_.pos), family)) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid source address";
    }
    return -1;
  }

  auto src_addr = rb_.pos;
  auto src_addrlen = token_end - rb_.pos;

  rb_.drain(token_end - rb_.pos + 1);

  // destination address
  token_end = std::find(rb_.pos, end, ' ');
  if (token_end == end) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Destination address not found";
    }
    return -1;
  }

  *token_end = '\0';
  if (!util::numeric_host(reinterpret_cast<const char *>(rb_.pos), family)) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid destination address";
    }
    return -1;
  }

  // Currently we don't use destination address

  rb_.drain(token_end - rb_.pos + 1);

  // source port
  auto n = parse_proxy_line_port(rb_.pos, end);
  if (n <= 0 || *(rb_.pos + n) != ' ') {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid source port";
    }
    return -1;
  }

  rb_.pos[n] = '\0';
  auto src_port = rb_.pos;
  auto src_portlen = n;

  rb_.drain(n + 1);

  // destination  port
  n = parse_proxy_line_port(rb_.pos, end);
  if (n <= 0 || rb_.pos + n != end) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid destination port";
    }
    return -1;
  }

  // Currently we don't use destination port

  rb_.drain(end + 2 - rb_.pos);

  ipaddr_.assign(src_addr, src_addr + src_addrlen);
  port_.assign(src_port, src_port + src_portlen);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol-v1: Finished, " << (rb_.pos - first)
                     << " bytes read";
  }

  return on_proxy_protocol_finish();
}

} // namespace shrpx

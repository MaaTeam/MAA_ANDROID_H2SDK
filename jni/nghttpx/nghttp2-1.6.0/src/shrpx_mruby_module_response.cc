/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#include "shrpx_mruby_module_response.h"

#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/hash.h>
#include <mruby/array.h>

#include "shrpx_downstream.h"
#include "shrpx_upstream.h"
#include "shrpx_client_handler.h"
#include "shrpx_mruby.h"
#include "shrpx_mruby_module.h"
#include "util.h"
#include "http2.h"

namespace shrpx {

namespace mruby {

namespace {
mrb_value response_init(mrb_state *mrb, mrb_value self) { return self; }
} // namespace

namespace {
mrb_value response_get_http_version_major(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  return mrb_fixnum_value(downstream->get_response_major());
}
} // namespace

namespace {
mrb_value response_get_http_version_minor(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  return mrb_fixnum_value(downstream->get_response_minor());
}
} // namespace

namespace {
mrb_value response_get_status(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;

  return mrb_fixnum_value(downstream->get_response_http_status());
}
} // namespace

namespace {
mrb_value response_set_status(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;

  mrb_int status;
  mrb_get_args(mrb, "i", &status);
  // We don't support 1xx status code for mruby scripting yet.
  if (status < 200 || status > 999) {
    mrb_raise(mrb, E_RUNTIME_ERROR,
              "invalid status; it should be [200, 999], inclusive");
  }

  downstream->set_response_http_status(status);

  return self;
}
} // namespace

namespace {
mrb_value response_get_headers(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  return create_headers_hash(mrb, downstream->get_response_headers());
}
} // namespace

namespace {
mrb_value response_mod_header(mrb_state *mrb, mrb_value self, bool repl) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;

  mrb_value key, values;
  mrb_get_args(mrb, "oo", &key, &values);

  if (RSTRING_LEN(key) == 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "empty key is not allowed");
  }

  key = mrb_funcall(mrb, key, "downcase", 0);

  if (repl) {
    size_t p = 0;
    auto &headers = downstream->get_response_headers();
    for (size_t i = 0; i < headers.size(); ++i) {
      auto &hd = headers[i];
      if (util::streq(std::begin(hd.name), hd.name.size(), RSTRING_PTR(key),
                      RSTRING_LEN(key))) {
        continue;
      }
      if (i != p) {
        headers[p++] = std::move(hd);
      }
    }
    headers.resize(p);
  }

  if (mrb_obj_is_instance_of(mrb, values, mrb->array_class)) {
    auto n = mrb_ary_len(mrb, values);
    for (int i = 0; i < n; ++i) {
      auto value = mrb_ary_entry(values, i);
      downstream->add_response_header(
          std::string(RSTRING_PTR(key), RSTRING_LEN(key)),
          std::string(RSTRING_PTR(value), RSTRING_LEN(value)));
    }
  } else if (!mrb_nil_p(values)) {
    downstream->add_response_header(
        std::string(RSTRING_PTR(key), RSTRING_LEN(key)),
        std::string(RSTRING_PTR(values), RSTRING_LEN(values)));
  }

  data->response_headers_dirty = true;

  return mrb_nil_value();
}
} // namespace

namespace {
mrb_value response_set_header(mrb_state *mrb, mrb_value self) {
  return response_mod_header(mrb, self, true);
}
} // namespace

namespace {
mrb_value response_add_header(mrb_state *mrb, mrb_value self) {
  return response_mod_header(mrb, self, false);
}
} // namespace

namespace {
mrb_value response_clear_headers(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;

  downstream->clear_response_headers();

  return mrb_nil_value();
}
} // namespace

namespace {
mrb_value response_return(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  int rv;

  if (downstream->get_response_state() == Downstream::MSG_COMPLETE) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "response has already been committed");
  }

  const char *val;
  mrb_int vallen;
  mrb_get_args(mrb, "|s", &val, &vallen);

  const uint8_t *body = nullptr;
  size_t bodylen = 0;

  if (downstream->get_response_http_status() == 0) {
    downstream->set_response_http_status(200);
  }

  if (data->response_headers_dirty) {
    downstream->index_response_headers();
    data->response_headers_dirty = false;
  }

  if (downstream->expect_response_body() && vallen > 0) {
    body = reinterpret_cast<const uint8_t *>(val);
    bodylen = vallen;
  }

  auto cl = downstream->get_response_header(http2::HD_CONTENT_LENGTH);
  if (cl) {
    cl->value = util::utos(bodylen);
  } else {
    downstream->add_response_header("content-length", util::utos(bodylen),
                                    http2::HD_CONTENT_LENGTH);
  }
  downstream->set_response_content_length(bodylen);

  auto date = downstream->get_response_header(http2::HD_DATE);
  if (!date) {
    auto lgconf = log_config();
    lgconf->update_tstamp(std::chrono::system_clock::now());
    downstream->add_response_header("date", lgconf->time_http_str,
                                    http2::HD_DATE);
  }

  auto upstream = downstream->get_upstream();

  rv = upstream->send_reply(downstream, body, bodylen);
  if (rv != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "could not send response");
  }

  auto handler = upstream->get_client_handler();

  handler->signal_write();

  return self;
}
} // namespace

void init_response_class(mrb_state *mrb, RClass *module) {
  auto response_class =
      mrb_define_class_under(mrb, module, "Response", mrb->object_class);

  mrb_define_method(mrb, response_class, "initialize", response_init,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "http_version_major",
                    response_get_http_version_major, MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "http_version_minor",
                    response_get_http_version_minor, MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "status", response_get_status,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "status=", response_set_status,
                    MRB_ARGS_REQ(1));
  mrb_define_method(mrb, response_class, "headers", response_get_headers,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "add_header", response_add_header,
                    MRB_ARGS_REQ(2));
  mrb_define_method(mrb, response_class, "set_header", response_set_header,
                    MRB_ARGS_REQ(2));
  mrb_define_method(mrb, response_class, "clear_headers",
                    response_clear_headers, MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "return", response_return,
                    MRB_ARGS_OPT(1));
}

} // namespace mruby

} // namespace shrpx

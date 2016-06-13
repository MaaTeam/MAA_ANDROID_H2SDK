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
#include "shrpx_http.h"

#include "shrpx_config.h"
#include "shrpx_log.h"
#include "http2.h"
#include "util.h"

using namespace nghttp2;

namespace shrpx {

namespace http {

std::string create_error_html(unsigned int status_code) {
  std::string res;
  res.reserve(512);
  auto status = http2::get_status_string(status_code);
  res += R"(<!DOCTYPE html><html lang="en"><title>)";
  res += status;
  res += "</title><body><h1>";
  res += status;
  res += "</h1><footer>";
  res += get_config()->server_name;
  res += " at port ";
  res += util::utos(get_config()->port);
  res += "</footer></body></html>";
  return res;
}

std::string create_via_header_value(int major, int minor) {
  std::string hdrs;
  hdrs += static_cast<char>(major + '0');
  if (major < 2) {
    hdrs += '.';
    hdrs += static_cast<char>(minor + '0');
  }
  hdrs += " nghttpx";
  return hdrs;
}

std::string colorizeHeaders(const char *hdrs) {
  std::string nhdrs;
  const char *p = strchr(hdrs, '\n');
  if (!p) {
    // Not valid HTTP header
    return hdrs;
  }
  nhdrs.append(hdrs, p + 1);
  ++p;
  while (1) {
    const char *np = strchr(p, ':');
    if (!np) {
      nhdrs.append(p);
      break;
    }
    nhdrs += TTY_HTTP_HD;
    nhdrs.append(p, np);
    nhdrs += TTY_RST;
    p = np;
    np = strchr(p, '\n');
    if (!np) {
      nhdrs.append(p);
      break;
    }
    nhdrs.append(p, np + 1);
    p = np + 1;
  }
  return nhdrs;
}

ssize_t select_padding_callback(nghttp2_session *session,
                                const nghttp2_frame *frame, size_t max_payload,
                                void *user_data) {
  return std::min(max_payload, frame->hd.length + get_config()->padding);
}

} // namespace http

} // namespace shrpx

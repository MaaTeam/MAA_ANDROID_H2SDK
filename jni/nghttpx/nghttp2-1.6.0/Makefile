TARGET = nghttpx
INCLUDES = \
	-I../libev-4.19/ \
	-I./ \
	-Ilib/includes/ \
	-Isrc/ \
	-Isrc/includes/ \
	-Ithird-party/ \
	-Ithird-party/http-parser/

CFLAGS = -D_U_="" -DHAVE_CONFIG_H=1 $(INCLUDES) -g

CXXFLAGS = -std=c++11  -fexceptions -frtti -DHAVE_CONFIG_H=1 -DPKGDATADIR="" $(INCLUDES) -g

DEPS = \
	lib/nghttp2_buf.o \
	lib/nghttp2_callbacks.o \
	lib/nghttp2_frame.o \
	lib/nghttp2_hd.o \
	lib/nghttp2_hd_huffman.o \
	lib/nghttp2_hd_huffman_data.o \
	lib/nghttp2_helper.o \
	lib/nghttp2_http.o \
	lib/nghttp2_map.o \
	lib/nghttp2_mem.o \
	lib/nghttp2_npn.o \
	lib/nghttp2_option.o \
	lib/nghttp2_outbound_item.o \
	lib/nghttp2_pq.o \
	lib/nghttp2_priority_spec.o \
	lib/nghttp2_queue.o \
	lib/nghttp2_session.o \
	lib/nghttp2_stream.o \
	lib/nghttp2_submit.o \
	lib/nghttp2_version.o \
	src/app_helper.o \
	src/http2.o \
	src/ngdns.o \
	src/shrpx.o \
	src/shrpx_accept_handler.o \
	src/shrpx_client_handler.o \
	src/shrpx_config.o \
	src/shrpx_connect_blocker.o \
	src/shrpx_connection.o \
	src/shrpx_connection_handler.o \
	src/shrpx_downstream.o \
	src/shrpx_downstream_connection.o \
	src/shrpx_downstream_connection_pool.o \
	src/shrpx_downstream_queue.o \
	src/shrpx_http.o \
	src/shrpx_http2_downstream_connection.o \
	src/shrpx_http2_session.o \
	src/shrpx_http2_upstream.o \
	src/shrpx_http_downstream_connection.o \
	src/shrpx_https_upstream.o \
	src/shrpx_io_control.o \
	src/shrpx_memcached_connection.o \
	src/shrpx_memcached_dispatcher.o \
	src/shrpx_log.o \
	src/shrpx_log_config.o \
	src/shrpx_rate_limit.o \
	src/shrpx_router.o \
	src/shrpx_signal.o \
	src/shrpx_ssl.o \
	src/shrpx_worker.o \
	src/shrpx_worker_process.o \
	src/ssl.o \
	src/timegm.o \
	src/util.o \
	third-party/http-parser/http_parser.o

$(TARGET): $(DEPS) ../libev-4.19/libev.a
	g++ -o $(TARGET) $(DEPS) ../libev-4.19/libev.a -lz  -lssl -lcrypto -lpthread

../libev-4.19/libev.a:
	make -C ../libev-4.19/

clean:
	rm -f $(TARGET) $(DEPS) ../libev-4.19/libev.a
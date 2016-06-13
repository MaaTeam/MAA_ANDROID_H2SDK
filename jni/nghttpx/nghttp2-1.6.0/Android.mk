LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := nghttpx

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../libev-4.19/ \
	$(LOCAL_PATH)/../openssl/include/ \
	$(LOCAL_PATH)/ \
	$(LOCAL_PATH)/lib/includes/ \
	$(LOCAL_PATH)/src/ \
	$(LOCAL_PATH)/src/includes/ \
	$(LOCAL_PATH)/third-party/ \
	$(LOCAL_PATH)/third-party/http-parser/

LOCAL_STATIC_LIBRARIES := \
	libev \
	libcrypto \
	libssl

LOCAL_CFLAGS := -D_U_="" -DHAVE_CONFIG_H=1

LOCAL_CXXFLAGS += -std=c++11  -fexceptions -frtti -DHAVE_CONFIG_H=1 -DPKGDATADIR=""

LOCAL_LDFLAGS += -lz  -llog

LOCAL_SRC_FILES := \
	lib/nghttp2_buf.c \
	lib/nghttp2_callbacks.c \
	lib/nghttp2_frame.c \
	lib/nghttp2_hd.c \
	lib/nghttp2_hd_huffman.c \
	lib/nghttp2_hd_huffman_data.c \
	lib/nghttp2_helper.c \
	lib/nghttp2_http.c \
	lib/nghttp2_map.c \
	lib/nghttp2_mem.c \
	lib/nghttp2_npn.c \
	lib/nghttp2_option.c \
	lib/nghttp2_outbound_item.c \
	lib/nghttp2_pq.c \
	lib/nghttp2_priority_spec.c \
	lib/nghttp2_queue.c \
	lib/nghttp2_session.c \
	lib/nghttp2_stream.c \
	lib/nghttp2_submit.c \
	lib/nghttp2_version.c \
	src/app_helper.cc \
	src/http2.cc \
	src/ngdns.c \
	src/shrpx.cc \
	src/shrpx_jni.cc \
	src/shrpx_accept_handler.cc \
	src/shrpx_client_handler.cc \
	src/shrpx_config.cc \
	src/shrpx_connect_blocker.cc \
	src/shrpx_connection.cc \
	src/shrpx_connection_handler.cc \
	src/shrpx_downstream.cc \
	src/shrpx_downstream_connection.cc \
	src/shrpx_downstream_connection_pool.cc \
	src/shrpx_downstream_queue.cc \
	src/shrpx_https_upstream.cc \
	src/shrpx_http.cc \
	src/shrpx_http2_downstream_connection.cc \
	src/shrpx_http2_session.cc \
	src/shrpx_http2_upstream.cc \
	src/shrpx_http_downstream_connection.cc \
	src/shrpx_io_control.cc \
	src/shrpx_memcached_connection.cc \
	src/shrpx_memcached_dispatcher.cc \
	src/shrpx_log.cc \
	src/shrpx_log_config.cc \
	src/shrpx_rate_limit.cc \
	src/shrpx_router.cc \
	src/shrpx_signal.cc \
	src/shrpx_ssl.cc \
	src/shrpx_worker.cc \
	src/shrpx_worker_process.cc \
	src/ssl.cc \
	src/timegm.c \
	src/util.cc \
	third-party/http-parser/http_parser.c

include $(BUILD_SHARED_LIBRARY)
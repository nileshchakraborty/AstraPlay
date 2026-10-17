// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_UTILS_H
#define CHIAKI_UTILS_H

#include <chiaki/sock.h>
#include <chiaki/log.h>
#include <chiaki/thread.h>
#include <chiaki/time.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>
#endif

#include <stdint.h>

static inline ChiakiErrorCode set_port(struct sockaddr *sa, uint16_t port)
{
	if(sa->sa_family == AF_INET)
		((struct sockaddr_in *)sa)->sin_port = port;
	else if(sa->sa_family == AF_INET6)
		((struct sockaddr_in6 *)sa)->sin6_port = port;
	else
		return CHIAKI_ERR_INVALID_DATA;
	return CHIAKI_ERR_SUCCESS;
}

static inline const char *sockaddr_str(struct sockaddr *addr, char *addr_buf, size_t addr_buf_size)
{
	void *addr_src;
	switch(addr->sa_family)
	{
		case AF_INET:
			addr_src = &((struct sockaddr_in *)addr)->sin_addr;
			break;
		case AF_INET6:
			addr_src = &((struct sockaddr_in6 *)addr)->sin6_addr;
			break;
		default:
			addr_src = NULL;
			break;
	}
	if(addr_src)
		return inet_ntop(addr->sa_family, addr_src, addr_buf, addr_buf_size);
	return NULL;
}

static inline int sendto_broadcast(ChiakiLog *log, chiaki_socket_t s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
#if defined(__FreeBSD__) || defined(__APPLE__)
	// FreeBSD, macOS, and iOS require sending to specific interface broadcast addresses
	// See: https://wiki.freebsd.org/NetworkRFCCompliance
	if(to->sa_family == AF_INET && ((const struct sockaddr_in *)to)->sin_addr.s_addr == htonl(INADDR_BROADCAST))
	{
		struct ifaddrs *ifap;
		if(getifaddrs(&ifap) < 0)
		{
			CHIAKI_LOGE(log, "Failed to getifaddrs for Broadcast: %s", strerror(errno));
			return -1;
		}
		int r = -1;
		for(struct ifaddrs *a=ifap; a; a=a->ifa_next)
		{
			if(!a->ifa_broadaddr)
				continue;
			if(!(a->ifa_flags & IFF_BROADCAST))
				continue;
			if(a->ifa_broadaddr->sa_family != to->sa_family)
				continue;
			((struct sockaddr_in *)a->ifa_broadaddr)->sin_port = ((const struct sockaddr_in *)to)->sin_port;
		char addr_buf[64];
		const char *addr_str = sockaddr_str(a->ifa_broadaddr, addr_buf, sizeof(addr_buf));
		// CHIAKI_LOGV(log, "Broadcast to %s on %s", addr_str ? addr_str : "(null)", a->ifa_name);
		int sr = sendto(s, msg, len, flags, a->ifa_broadaddr, sizeof(*a->ifa_broadaddr));
		if(sr < 0)
		{
			CHIAKI_LOGV(log, "Broadcast on iface %s failed: %s", a->ifa_name, strerror(errno));
			continue;
		}
			r = sr;
		}
		freeifaddrs(ifap);
		return r;
	}
#endif
	return sendto(s, (CHIAKI_SOCKET_BUF_TYPE) msg, len, flags, to, tolen);
}

static inline void xor_bytes(uint8_t *dst, uint8_t *src, size_t sz)
{
	while(sz > 0)
	{
		*dst ^= *src;
		dst++;
		src++;
		sz--;
	}
}

static inline int8_t nibble_value(char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 0xa;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 0xa;
	return -1;
}

static inline ChiakiErrorCode parse_hex(uint8_t *buf, size_t *buf_size, const char *hex, size_t hex_size)
{
	if(hex_size % 2 != 0)
		return CHIAKI_ERR_INVALID_DATA;
	if(hex_size / 2 > *buf_size)
		return CHIAKI_ERR_BUF_TOO_SMALL;

	for(size_t i=0; i<hex_size; i+=2)
	{
		int8_t h = nibble_value(hex[i+0]);
		if(h < 0)
			return CHIAKI_ERR_INVALID_DATA;
		int8_t l = nibble_value(hex[i+1]);
		if(l < 0)
			return CHIAKI_ERR_INVALID_DATA;
		buf[i/2] = (h << 4) | l;
	}

	*buf_size = hex_size / 2;
	return CHIAKI_ERR_SUCCESS;
}

static inline char nibble_char(uint8_t v)
{
	if(v > 0xf)
		return '0';
	if(v < 0xa)
		return '0' + v;
	return 'a' + v;
}

static inline ChiakiErrorCode format_hex(char *hex_buf, size_t hex_buf_size, const uint8_t *buf, size_t buf_size)
{
	if(hex_buf_size < buf_size * 2 + 1)
		return CHIAKI_ERR_BUF_TOO_SMALL;

	for(size_t i=0; i<buf_size; i++)
	{
		uint8_t v = buf[i];
		hex_buf[i*2+0] = nibble_char(v >> 4);
		hex_buf[i*2+1] = nibble_char(v & 0xf);
	}
	hex_buf[buf_size*2] = '\0';

	return CHIAKI_ERR_SUCCESS;
}

typedef struct ChiakiDnsResolveContext
{
	char *host;
	struct addrinfo hints;
	struct addrinfo *addrinfos;
	int result;
	bool finished;
	bool abandoned;
	ChiakiMutex mutex;
	ChiakiCond cond;
} ChiakiDnsResolveContext;

static inline void *chiaki_dns_resolve_worker(void *arg)
{
	ChiakiDnsResolveContext *ctx = (ChiakiDnsResolveContext *)arg;
	struct addrinfo *res = NULL;
	int r = getaddrinfo(ctx->host, NULL, &ctx->hints, &res);

	chiaki_mutex_lock(&ctx->mutex);
	if (ctx->abandoned)
	{
		if (res)
			freeaddrinfo(res);
		chiaki_mutex_unlock(&ctx->mutex);
		chiaki_cond_fini(&ctx->cond);
		chiaki_mutex_fini(&ctx->mutex);
		free(ctx->host);
		free(ctx);
		return NULL;
	}

	ctx->result = r;
	ctx->addrinfos = res;
	ctx->finished = true;
	chiaki_cond_signal(&ctx->cond);
	chiaki_mutex_unlock(&ctx->mutex);
	return NULL;
}

static inline int chiaki_resolve_host_async(ChiakiLog *log, const char *host, const struct addrinfo *hints, struct addrinfo **out_addrinfos, uint64_t timeout_ms)
{
	struct addrinfo local_hints = *hints;
	// Fast path: if host is already a numeric IPv4 or IPv6 literal, resolve synchronously without DNS lookup
	struct in_addr in4;
	struct in6_addr in6;
	if (inet_pton(AF_INET, host, &in4) == 1)
	{
		local_hints.ai_family = AF_INET;
		local_hints.ai_flags |= AI_NUMERICHOST;
		return getaddrinfo(host, NULL, &local_hints, out_addrinfos);
	}
	else if (inet_pton(AF_INET6, host, &in6) == 1)
	{
		local_hints.ai_family = AF_INET6;
		local_hints.ai_flags |= AI_NUMERICHOST;
		return getaddrinfo(host, NULL, &local_hints, out_addrinfos);
	}

	// Hostname resolution: run in background worker thread with timeout to prevent stalling event/discovery loops
	ChiakiDnsResolveContext *ctx = (ChiakiDnsResolveContext *)calloc(1, sizeof(ChiakiDnsResolveContext));
	if (!ctx)
		return getaddrinfo(host, NULL, &local_hints, out_addrinfos);

	ctx->host = strdup(host);
	if (!ctx->host)
	{
		free(ctx);
		return getaddrinfo(host, NULL, &local_hints, out_addrinfos);
	}
	ctx->hints = local_hints;
	chiaki_mutex_init(&ctx->mutex, false);
	chiaki_cond_init(&ctx->cond);

	ChiakiThread thread;
	ChiakiErrorCode terr = chiaki_thread_create(&thread, chiaki_dns_resolve_worker, ctx);
	if (terr != CHIAKI_ERR_SUCCESS)
	{
		chiaki_cond_fini(&ctx->cond);
		chiaki_mutex_fini(&ctx->mutex);
		free(ctx->host);
		free(ctx);
		return getaddrinfo(host, NULL, &local_hints, out_addrinfos);
	}

	chiaki_mutex_lock(&ctx->mutex);
	ChiakiErrorCode wait_err = CHIAKI_ERR_SUCCESS;
	uint64_t deadline = chiaki_time_now_monotonic_ms() + timeout_ms;
	while (!ctx->finished && wait_err == CHIAKI_ERR_SUCCESS)
	{
		uint64_t now = chiaki_time_now_monotonic_ms();
		if (now >= deadline)
			break;
		wait_err = chiaki_cond_timedwait(&ctx->cond, &ctx->mutex, deadline - now);
	}

	if (ctx->finished)
	{
		chiaki_mutex_unlock(&ctx->mutex);
		chiaki_thread_join(&thread, NULL);
		int r = ctx->result;
		*out_addrinfos = ctx->addrinfos;
		chiaki_cond_fini(&ctx->cond);
		chiaki_mutex_fini(&ctx->mutex);
		free(ctx->host);
		free(ctx);
		return r;
	}
	else
	{
		ctx->abandoned = true;
		chiaki_mutex_unlock(&ctx->mutex);
		chiaki_thread_detach(&thread);
		CHIAKI_LOGE(log, "DNS resolution timed out after %llu ms for '%s'", (unsigned long long)timeout_ms, host);
		return EAI_AGAIN;
	}
}

#endif // CHIAKI_UTILS_H

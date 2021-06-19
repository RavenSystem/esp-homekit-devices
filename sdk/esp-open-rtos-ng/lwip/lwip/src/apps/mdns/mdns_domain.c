/**
 * @file
 * MDNS responder implementation - domain related functionalities
 */

/*
 * Copyright (c) 2015 Verisure Innovation AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Erik Ekman <erik@kryo.se>
 * Author: Jasper Verschueren <jasper.verschueren@apart-audio.com>
 *
 */

#include "lwip/apps/mdns.h"
#include "lwip/apps/mdns_domain.h"
#include "lwip/apps/mdns_priv.h"
#include "lwip/prot/dns.h"

#include <string.h>

#if LWIP_IPV6
#include "lwip/prot/ip6.h"
#endif

#if LWIP_MDNS_RESPONDER

/* Stored offsets to beginning of domain names
 * Used for compression.
 */
#define DOMAIN_JUMP_SIZE 2
#define DOMAIN_JUMP 0xc000

#define TOPDOMAIN_LOCAL "local"

#define REVERSE_PTR_TOPDOMAIN "arpa"
#define REVERSE_PTR_V4_DOMAIN "in-addr"
#define REVERSE_PTR_V6_DOMAIN "ip6"

static const char *dnssd_protos[] = {
  "_udp", /* DNSSD_PROTO_UDP */
  "_tcp", /* DNSSD_PROTO_TCP */
};

/* forward declarations (function prototypes)*/
static err_t mdns_domain_add_label_base(struct mdns_domain *domain, u8_t len);
static err_t mdns_domain_add_label_pbuf(struct mdns_domain *domain,
                                        const struct pbuf *p, u16_t offset,
                                        u8_t len);
static u16_t mdns_readname_loop(struct pbuf *p, u16_t offset,
                                struct mdns_domain *domain, unsigned depth);
static err_t mdns_add_dotlocal(struct mdns_domain *domain);

struct mdns_domain *
mdns_domain_alloc(void)
{
  return (struct mdns_domain *)mem_calloc(1, sizeof(struct mdns_domain));
}

void
mdns_domain_free(struct mdns_domain *domain)
{
  u8_t *name = domain->name;
  if (name) {
    domain->name = NULL;
    mem_free(name);
  }
  mem_free(domain);
}

err_t
mdns_domain_ensure_name(struct mdns_domain *domain, u16_t length)
{
  LWIP_ASSERT("mdns_domain_ensure_name: length overflow", length <= MDNS_DOMAIN_MAXLEN);
  if (!domain->name) {
    domain->name = (u8_t *)mem_malloc(length);
    if (!domain->name) {
      return ERR_MEM;
    }
    domain->storage_length = length;
  } else if (length > domain->storage_length) {
    u8_t *new_name = (u8_t *)mem_malloc(length);
    if (!new_name) {
      return ERR_MEM;
    }
    memcpy(new_name, domain->name, domain->storage_length);
    mem_free(domain->name);
    domain->name = new_name;
    domain->storage_length = length;
  }
  return ERR_OK;
}

static err_t
mdns_domain_add_label_base(struct mdns_domain *domain, u8_t len)
{
  u16_t required;
  if (len > MDNS_LABEL_MAXLEN) {
    return ERR_VAL;
  }
  required = domain->length + 1 + len;
  if (len > 0) {
    err_t err;
    if (required >= MDNS_DOMAIN_MAXLEN) {
      return ERR_VAL;
    }
    /* Allocate space for the expected zero marker. */
    err = mdns_domain_ensure_name(domain, required + 1);
    if (err != ERR_OK) {
      return err;
    }
  } else {
    /* Zero len. Allow only zero marker on last byte */
    err_t err;
    if (required > MDNS_DOMAIN_MAXLEN) {
      return ERR_VAL;
    }
    err = mdns_domain_ensure_name(domain, required);
    if (err != ERR_OK) {
      return err;
    }
  }

  domain->name[domain->length] = len;
  domain->length++;
  return ERR_OK;
}

/**
 * Add a label part to a domain
 * @param domain The domain to add a label to
 * @param label The label to add, like &lt;hostname&gt;, 'local', 'com' or ''
 * @param len The length of the label
 * @return ERR_OK on success, an err_t otherwise if label too long
 */
err_t
mdns_domain_add_label(struct mdns_domain *domain, const char *label, u8_t len)
{
  err_t err = mdns_domain_add_label_base(domain, len);
  if (err != ERR_OK) {
    return err;
  }
  if (len) {
    MEMCPY(&domain->name[domain->length], label, len);
    domain->length += len;
  }
  return ERR_OK;
}

/**
 * Add a label part to a domain (@see mdns_domain_add_label but copy directly from pbuf)
 */
static err_t
mdns_domain_add_label_pbuf(struct mdns_domain *domain, const struct pbuf *p, u16_t offset, u8_t len)
{
  err_t err = mdns_domain_add_label_base(domain, len);
  if (err != ERR_OK) {
    return err;
  }
  if (len) {
    if (pbuf_copy_partial(p, &domain->name[domain->length], len, offset) != len) {
      /* take back the ++ done before */
      domain->length--;
      return ERR_ARG;
    }
    domain->length += len;
  }
  return ERR_OK;
}

/**
 * Add a partial domain to a domain
 * @param domain The domain to add a label to
 * @param source The domain to add, like &lt;\\x09_services\\007_dns-sd\\000&gt;
 * @return ERR_OK on success, an err_t otherwise if label too long
 */
err_t
mdns_domain_add_domain(struct mdns_domain *domain, struct mdns_domain *source)
{
  u16_t len;
  u16_t required;

  len = source->length;
  required = domain->length + len + 1;

  if (len > 0) {
    err_t err;
    if (required >= MDNS_DOMAIN_MAXLEN) {
      return ERR_VAL;
    }
    /* Allocate space for the expected zero marker. */
    err = mdns_domain_ensure_name(domain, required + 1);
    if (err != ERR_OK) {
      return err;
    }
    /* Copy partial domain */
    MEMCPY(&domain->name[domain->length], source->name, len);
    domain->length += len;
  } else {
    err_t err;
    /* Zero len. Allow only zero marker on last byte */
    if (required > MDNS_DOMAIN_MAXLEN) {
      return ERR_VAL;
    }
    err = mdns_domain_ensure_name(domain, required);
    if (err != ERR_OK) {
      return err;
    }
    /* Add zero marker */
    domain->name[domain->length] = 0;
    domain->length++;
  }
  return ERR_OK;
}

/**
 * Add a string domain to a domain
 * @param domain The domain to add a label to
 * @param source The string to add, like &lt;_services._dns-sd&gt;
 * @return ERR_OK on success, an err_t otherwise if label too long
 */
err_t
mdns_domain_add_string(struct mdns_domain *domain, const char *source)
{
  u16_t source_len;
  u16_t required;
  err_t err;
  u8_t *len;
  u8_t *start;

  source_len = (u16_t)strnlen(source, MDNS_DOMAIN_MAXLEN);
  if (source_len >= MDNS_DOMAIN_MAXLEN) {
    return ERR_VAL;
  }
  required = domain->length + 1 + source_len;
  if (required >= MDNS_DOMAIN_MAXLEN) {
    return ERR_VAL;
  }
  /* Allocate space for the expected zero marker. */
  err = mdns_domain_ensure_name(domain, required + 1);
  if (err != ERR_OK) {
    return err;
  }

  len = &domain->name[domain->length];
  *len = 0;
  start = len + 1;
  while (*source) {
      if (*source == '.') {
        len = start++;
        *len = 0;
        source++;
      } else {
        *start++ = *source++;
        *len = *len + 1;
      }
  }
  LWIP_ASSERT("mdns_domain_add_string", required == (u16_t)(start - &domain->name[0]));
  domain->length = required;
  return ERR_OK;
}


/**
 * Internal readname function with max 6 levels of recursion following jumps
 * while decompressing name
 */
static u16_t
mdns_readname_loop(struct pbuf *p, u16_t offset, struct mdns_domain *domain, unsigned depth)
{
  u8_t c;

  do {
    if (depth > 5) {
      /* Too many jumps */
      return MDNS_READNAME_ERROR;
    }

    c = pbuf_get_at(p, offset);
    offset++;

    /* is this a compressed label? */
    if ((c & 0xc0) == 0xc0) {
      u16_t jumpaddr;
      if (offset >= p->tot_len) {
        /* Make sure both jump bytes fit in the packet */
        return MDNS_READNAME_ERROR;
      }
      jumpaddr = (((c & 0x3f) << 8) | (pbuf_get_at(p, offset) & 0xff));
      offset++;
      if (jumpaddr >= SIZEOF_DNS_HDR && jumpaddr < p->tot_len) {
        u16_t res;
        /* Recursive call, maximum depth will be checked */
        res = mdns_readname_loop(p, jumpaddr, domain, depth + 1);
        /* Dont return offset since new bytes were not read (jumped to somewhere in packet) */
        if (res == MDNS_READNAME_ERROR) {
          return res;
        }
      } else {
        return MDNS_READNAME_ERROR;
      }
      break;
    }

    /* normal label */
    if (c <= MDNS_LABEL_MAXLEN) {
      err_t res;

      if (c + domain->length >= MDNS_DOMAIN_MAXLEN) {
        return MDNS_READNAME_ERROR;
      }
      res = mdns_domain_add_label_pbuf(domain, p, offset, c);
      if (res != ERR_OK) {
        return MDNS_READNAME_ERROR;
      }
      offset += c;
    } else {
      /* bad length byte */
      return MDNS_READNAME_ERROR;
    }
  } while (c != 0);

  return offset;
}

/**
 * Read possibly compressed domain name from packet buffer
 * @param p The packet
 * @param offset start position of domain name in packet
 * @param domain The domain name destination
 * @return The new offset after the domain, or MDNS_READNAME_ERROR
 *         if reading failed
 */
u16_t
mdns_readname(struct pbuf *p, u16_t offset, struct mdns_domain **domain)
{
  *domain = mdns_domain_alloc();
  if (*domain == NULL) {
    return MDNS_READNAME_ERROR;
  }
  return mdns_readname_loop(p, offset, *domain, 0);
}

/**
 * Print domain name to debug output
 * @param domain The domain name
 */
void
mdns_domain_debug_print(struct mdns_domain *domain)
{
  u8_t *src = domain->name;
  u8_t i;

  while (*src) {
    u8_t label_len = *src;
    src++;
    for (i = 0; i < label_len; i++) {
      LWIP_DEBUGF(MDNS_DEBUG, ("%c", src[i]));
    }
    src += label_len;
    LWIP_DEBUGF(MDNS_DEBUG, ("."));
  }
}

/**
 * Return 1 if contents of domains match (case-insensitive)
 * @param a Domain name to compare 1
 * @param b Domain name to compare 2
 * @return 1 if domains are equal ignoring case, 0 otherwise
 */
int
mdns_domain_eq(struct mdns_domain *a, struct mdns_domain *b)
{
  u8_t *ptra, *ptrb;
  u8_t len;
  int res;

  if (a->length != b->length) {
    return 0;
  }

  ptra = a->name;
  ptrb = b->name;

  if (!ptra || !ptrb) {
    /* Consider them undefined */
    return 0;
  }

  while (*ptra && *ptrb && ptra < &a->name[a->length]) {
    if (*ptra != *ptrb) {
      return 0;
    }
    len = *ptra;
    ptra++;
    ptrb++;
    res = lwip_strnicmp((char *) ptra, (char *) ptrb, len);
    if (res != 0) {
      return 0;
    }
    ptra += len;
    ptrb += len;
  }
  if (*ptra != *ptrb && ptra < &a->name[a->length]) {
    return 0;
  }
  return 1;
}

#if LWIP_IPV4
/**
 * Build domain for reverse lookup of IPv4 address
 * like 12.0.168.192.in-addr.arpa. for 192.168.0.12
 * @param addr Pointer to an IPv4 address to encode
 * @return a struct pointer on success, otherwise NULL.
 */
struct mdns_domain *
mdns_build_reverse_v4_domain(const ip4_addr_t *addr)
{
  int i;
  err_t res;
  const u8_t *ptr;
  struct mdns_domain *domain;

  LWIP_UNUSED_ARG(res);
  if (!addr) {
    return NULL;
  }
  domain = mdns_domain_alloc();
  if (domain == NULL) {
    return NULL;
  }

  ptr = (const u8_t *) addr;
  for (i = sizeof(ip4_addr_t) - 1; i >= 0; i--) {
    char buf[4];
    u8_t val = ptr[i];

    lwip_itoa(buf, sizeof(buf), val);
    res = mdns_domain_add_label(domain, buf, (u8_t)strlen(buf));
    LWIP_ERROR("mdns_build_reverse_v4_domain: Failed to add label", (res == ERR_OK), goto err);
  }
  res = mdns_domain_add_label(domain, REVERSE_PTR_V4_DOMAIN, (u8_t)(sizeof(REVERSE_PTR_V4_DOMAIN) - 1));
  LWIP_ERROR("mdns_build_reverse_v4_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, REVERSE_PTR_TOPDOMAIN, (u8_t)(sizeof(REVERSE_PTR_TOPDOMAIN) - 1));
  LWIP_ERROR("mdns_build_reverse_v4_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, NULL, 0);
  LWIP_ERROR("mdns_build_reverse_v4_domain: Failed to add label", (res == ERR_OK), goto err);
  return domain;

 err:
  mdns_domain_free(domain);
  return NULL;
}
#endif

#if LWIP_IPV6
/**
 * Build domain for reverse lookup of IP address
 * like b.a.9.8.7.6.5.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa. for 2001:db8::567:89ab
 * @param addr Pointer to an IPv6 address to encode
 * @return a struct pointer on success, otherwise NULL.
 */
struct mdns_domain *
mdns_build_reverse_v6_domain(const ip6_addr_t *addr)
{
  int i;
  err_t res;
  const u8_t *ptr;
  struct mdns_domain *domain;
  LWIP_UNUSED_ARG(res);

  if (!addr) {
    return NULL;
  }
  domain = mdns_domain_alloc();
  if (domain == NULL) {
    return NULL;
  }
  ptr = (const u8_t *) addr;
  for (i = sizeof(ip6_addr_p_t) - 1; i >= 0; i--) {
    char buf;
    u8_t byte = ptr[i];
    int j;
    for (j = 0; j < 2; j++) {
      if ((byte & 0x0F) < 0xA) {
        buf = '0' + (byte & 0x0F);
      } else {
        buf = 'a' + (byte & 0x0F) - 0xA;
      }
      res = mdns_domain_add_label(domain, &buf, sizeof(buf));
      LWIP_ERROR("mdns_build_reverse_v6_domain: Failed to add label", (res == ERR_OK), goto err);
      byte >>= 4;
    }
  }
  res = mdns_domain_add_label(domain, REVERSE_PTR_V6_DOMAIN, (u8_t)(sizeof(REVERSE_PTR_V6_DOMAIN) - 1));
  LWIP_ERROR("mdns_build_reverse_v6_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, REVERSE_PTR_TOPDOMAIN, (u8_t)(sizeof(REVERSE_PTR_TOPDOMAIN) - 1));
  LWIP_ERROR("mdns_build_reverse_v6_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, NULL, 0);
  LWIP_ERROR("mdns_build_reverse_v6_domain: Failed to add label", (res == ERR_OK), goto err);

  return domain;

 err:
  mdns_domain_free(domain);
  return NULL;
}
#endif

/* Add .local. to domain */
static err_t
mdns_add_dotlocal(struct mdns_domain *domain)
{
  err_t res = mdns_domain_add_label(domain, TOPDOMAIN_LOCAL, (u8_t)(sizeof(TOPDOMAIN_LOCAL) - 1));
  LWIP_UNUSED_ARG(res);
  LWIP_ERROR("mdns_add_dotlocal: Failed to add label", (res == ERR_OK), return res);
  return mdns_domain_add_label(domain, NULL, 0);
}

/**
 * Build the \<hostname\>.local. domain name
 * @param mdns TMDNS netif descriptor.
 * @return a struct pointer if domain \<hostname\>.local. was written, otherwise NULL.
 */
struct mdns_domain *
mdns_build_host_domain(struct mdns_host *mdns)
{
  err_t res;
  struct mdns_domain *domain;
  LWIP_UNUSED_ARG(res);
  domain = mdns_domain_alloc();
  if (domain == NULL) {
    return NULL;
  }
  LWIP_ERROR("mdns_build_host_domain: mdns != NULL", (mdns != NULL), goto err);
  res = mdns_domain_add_label(domain, mdns->name, (u8_t)strlen(mdns->name));
  LWIP_ERROR("mdns_build_host_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_add_dotlocal(domain);
  LWIP_ERROR("mdns_build_host_domain: Failed to add dot", (res == ERR_OK), goto err);
  return domain;

 err:
  mdns_domain_free(domain);
  return NULL;
}

/**
 * Build the lookup-all-services special DNS-SD domain name
 * @return a struct pointer if domain _services._dns-sd._udp.local. was written, otherwise NULL.
 */
struct mdns_domain *
mdns_build_dnssd_domain(void)
{
  err_t res;
  struct mdns_domain *domain;
  LWIP_UNUSED_ARG(res);
  domain = mdns_domain_alloc();
  if (domain == NULL) {
    return NULL;
  }
  res = mdns_domain_add_label(domain, "_services", (u8_t)(sizeof("_services") - 1));
  LWIP_ERROR("mdns_build_dnssd_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, "_dns-sd", (u8_t)(sizeof("_dns-sd") - 1));
  LWIP_ERROR("mdns_build_dnssd_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, dnssd_protos[DNSSD_PROTO_UDP], (u8_t)strlen(dnssd_protos[DNSSD_PROTO_UDP]));
  LWIP_ERROR("mdns_build_dnssd_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_add_dotlocal(domain);
  LWIP_ERROR("mdns_build_dnssd_domain: Failed to add dot", (res == ERR_OK), goto err);
  return domain;

 err:
  mdns_domain_free(domain);
  return NULL;
}

/**
 * Build domain name for a service
 * @param service The service struct, containing service name, type and protocol
 * @param include_name Whether to include the service name in the domain
 * @return a struct pointer if domain was written. If service name is included,
 *         \<name\>.\<type\>.\<proto\>.local. will be written, otherwise \<type\>.\<proto\>.local.
 *         NULL is returned on error.
 */
struct mdns_domain *
mdns_build_service_domain(struct mdns_service *service, int include_name)
{
  err_t res;
  struct mdns_domain *domain;
  LWIP_UNUSED_ARG(res);
  domain = mdns_domain_alloc();
  if (domain == NULL) {
    return NULL;
  }
  if (include_name) {
    res = mdns_domain_add_label(domain, service->name, (u8_t)strlen(service->name));
    LWIP_ERROR("mdns_build_service_domain: Failed to add label", (res == ERR_OK), goto err);
  }
  res = mdns_domain_add_label(domain, service->service, (u8_t)strlen(service->service));
  LWIP_ERROR("mdns_build_service_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, dnssd_protos[service->proto], (u8_t)strlen(dnssd_protos[service->proto]));
  LWIP_ERROR("mdns_build_service_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_add_dotlocal(domain);
  LWIP_ERROR("mdns_build_service_domain: Failed to add dot", (res == ERR_OK), goto err);
  return domain;

 err:
  mdns_domain_free(domain);
  return NULL;
}

#if LWIP_MDNS_SEARCH
/**
 * Build domain name for a request
 * @param request The request struct, containing service name, type and protocol
 * @param include_name Whether to include the service name in the domain
 * @return a struct pointer if domain was written. If service name is included,
 *         \<name\>.\<type\>.\<proto\>.local. will be written, otherwise \<type\>.\<proto\>.local.
 */
struct mdns_domain *
mdns_build_request_domain(struct mdns_request *request, int include_name)
{
  err_t res;
  struct mdns_domain *domain;
  LWIP_UNUSED_ARG(res);
  domain = mdns_domain_alloc();
  if (domain == NULL) {
    return NULL;
  }
  if (include_name) {
    res = mdns_domain_add_label(domain, request->name, (u8_t)strlen(request->name));
    LWIP_ERROR("mdns_build_request_domain: Failed to add label", (res == ERR_OK), goto err);
  }
  res = mdns_domain_add_domain(domain, request->service);
  LWIP_ERROR("mdns_build_request_domain: Failed to add domain", (res == ERR_OK), goto err);
  res = mdns_domain_add_label(domain, dnssd_protos[request->proto], (u8_t)strlen(dnssd_protos[request->proto]));
  LWIP_ERROR("mdns_build_request_domain: Failed to add label", (res == ERR_OK), goto err);
  res = mdns_add_dotlocal(domain);
  LWIP_ERROR("mdns_build_service_domain: Failed to add dot", (res == ERR_OK), goto err);
  return domain;

 err:
  mdns_domain_free(domain);
  return NULL;
}
#endif

/**
 * Return bytes needed to write before jump for best result of compressing supplied domain
 * against domain in outpacket starting at specified offset.
 * If a match is found, offset is updated to where to jump to
 * @param pbuf Pointer to pbuf with the partially constructed DNS packet
 * @param offset Start position of a domain written earlier. If this location is suitable
 *               for compression, the pointer is updated to where in the domain to jump to.
 * @param domain The domain to write
 * @return Number of bytes to write of the new domain before writing a jump to the offset.
 *         If compression can not be done against this previous domain name, the full new
 *         domain length is returned.
 */
u16_t
mdns_compress_domain(struct pbuf *pbuf, u16_t *offset, struct mdns_domain *domain)
{
  struct mdns_domain *target;
  u16_t target_end;
  u8_t target_len;
  u8_t writelen = 0;
  u8_t *ptr;
  if (pbuf == NULL) {
    return domain->length;
  }
  target_end = mdns_readname(pbuf, *offset, &target);
  if (target_end == MDNS_READNAME_ERROR) {
    mdns_domain_free(target);
    return domain->length;
  }
  target_len = (u8_t)(target_end - *offset);
  ptr = domain->name;
  if (!ptr) {
    return domain->length;
  }
  while (writelen < domain->length) {
    u8_t domainlen = (u8_t)(domain->length - writelen);
    u8_t labellen;
    if (domainlen <= target->length && domainlen > DOMAIN_JUMP_SIZE) {
      /* Compare domains if target is long enough, and we have enough left of the domain */
      u8_t targetpos = (u8_t)(target->length - domainlen);
      if ((targetpos + DOMAIN_JUMP_SIZE) >= target_len) {
        /* We are checking at or beyond a jump in the original, stop looking */
        break;
      }
      if (target->length >= domainlen &&
          memcmp(&domain->name[writelen], &target->name[targetpos], domainlen) == 0) {
        *offset += targetpos;
        mdns_domain_free(target);
        return writelen;
      }
    }
    /* Skip to next label in domain */
    labellen = *ptr;
    writelen += 1 + labellen;
    ptr += 1 + labellen;
  }
  /* Nothing found */
  mdns_domain_free(target);
  return domain->length;
}

/**
 * Write domain to outpacket. Compression will be attempted,
 * unless domain->skip_compression is set.
 * @param outpkt The outpacket to write to
 * @param domain The domain name to write
 * @return ERR_OK on success, an err_t otherwise
 */
err_t
mdns_write_domain(struct mdns_outpacket *outpkt, struct mdns_domain *domain)
{
  int i;
  err_t res;
  u16_t writelen = domain->length;
  u16_t jump_offset = 0;
  u16_t jump;

  if (!domain->skip_compression) {
    for (i = 0; i < NUM_DOMAIN_OFFSETS; i++) {
      u16_t offset = outpkt->domain_offsets[i];
      if (offset) {
        u16_t len = mdns_compress_domain(outpkt->pbuf, &offset, domain);
        if (len < writelen) {
          writelen = len;
          jump_offset = offset;
        }
      }
    }
  }

  if (writelen) {
    /* Write uncompressed part of name */
    res = pbuf_take_at(outpkt->pbuf, domain->name, writelen, outpkt->write_offset);
    if (res != ERR_OK) {
      return res;
    }

    /* Store offset of this new domain */
    for (i = 0; i < NUM_DOMAIN_OFFSETS; i++) {
      if (outpkt->domain_offsets[i] == 0) {
        outpkt->domain_offsets[i] = outpkt->write_offset;
        break;
      }
    }

    outpkt->write_offset += writelen;
  }
  if (jump_offset) {
    /* Write jump */
    jump = lwip_htons(DOMAIN_JUMP | jump_offset);
    res = pbuf_take_at(outpkt->pbuf, &jump, DOMAIN_JUMP_SIZE, outpkt->write_offset);
    if (res != ERR_OK) {
      return res;
    }
    outpkt->write_offset += DOMAIN_JUMP_SIZE;
  }
  return ERR_OK;
}

#endif /* LWIP_MDNS_RESPONDER */

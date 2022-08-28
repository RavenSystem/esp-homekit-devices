/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
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
 * Author: Simon Goldschmidt
 *
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#define ESP_OPEN_RTOS                       1

/* See tcp.c tcp_alloc(). */
#ifndef ESP_TIMEWAIT_THRESHOLD
#define ESP_TIMEWAIT_THRESHOLD              10000
#endif

/** LWIP_TIMEVAL_PRIVATE: if you want to use the struct timeval provided
 * by your system, set this to 0 and include <sys/time.h> in cc.h */
#define LWIP_TIMEVAL_PRIVATE                0

/*
   -----------------------------------------------
   ---------- Platform specific locking ----------
   -----------------------------------------------
*/
/**
 * SYS_LIGHTWEIGHT_PROT==1: if you want inter-task protection for certain
 * critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT        1

/**
 * MEMCPY: override this if you have a faster implementation at hand than the
 * one included in your C library
 */
#define MEMCPY(dst,src,len)             memcpy(dst,src,len)

/**
 * SMEMCPY: override this with care! Some compilers (e.g. gcc) can inline a
 * call to memcpy() if the length is known at compile time and is small.
 */
#define SMEMCPY(dst,src,len)            memcpy(dst,src,len)

/*
   ------------------------------------
   ----------- Core locking -----------
   ------------------------------------
*/

/**
 * LWIP_TCPIP_CORE_LOCKING
 * Creates a global mutex that is held during TCPIP thread operations.
 * Can be locked by client code to perform lwIP operations without changing
 * into TCPIP thread using callbacks. See LOCK_TCPIP_CORE() and
 * UNLOCK_TCPIP_CORE().
 * Your system should provide mutexes supporting priority inversion to use this.
 */
#ifndef LWIP_TCPIP_CORE_LOCKING
#define LWIP_TCPIP_CORE_LOCKING         1
#endif

/**
 * LWIP_TCPIP_CORE_LOCKING_INPUT: when LWIP_TCPIP_CORE_LOCKING is enabled,
 * this lets tcpip_input() grab the mutex for input packets as well,
 * instead of allocating a message and passing it to tcpip_thread.
 *
 * ATTENTION: this does not work when tcpip_input() is called from
 * interrupt context!
 */
#ifndef LWIP_TCPIP_CORE_LOCKING_INPUT
#define LWIP_TCPIP_CORE_LOCKING_INPUT   0
#endif

/**
 * Macro/function to check whether lwIP's threading/locking
 * requirements are satisfied during current function call.
 * This macro usually calls a function that is implemented in the OS-dependent
 * sys layer and performs the following checks:
 * - Not in ISR
 * - If @ref LWIP_TCPIP_CORE_LOCKING = 1: TCPIP core lock is held
 * - If @ref LWIP_TCPIP_CORE_LOCKING = 0: function is called from TCPIP thread
 * @see @ref multithreading
 */
#ifndef LWIP_ASSERT_CORE_LOCKED
void sys_check_core_locking(void);
#define LWIP_ASSERT_CORE_LOCKED()       sys_check_core_locking()
#endif

/**
 * Called as first thing in the lwIP TCPIP thread. Can be used in conjunction
 * with @ref LWIP_ASSERT_CORE_LOCKED to check core locking.
 * @see @ref multithreading
 */
#ifndef LWIP_MARK_TCPIP_THREAD
void sys_mark_tcpip_thread(void);
#define LWIP_MARK_TCPIP_THREAD()        sys_mark_tcpip_thread()
#endif

#if LWIP_TCPIP_CORE_LOCKING

#ifndef LOCK_TCPIP_CORE
void sys_lock_tcpip_core(void);
#define LOCK_TCPIP_CORE()          sys_lock_tcpip_core()
#endif

#ifndef UNLOCK_TCPIP_CORE
void sys_unlock_tcpip_core(void);
#define UNLOCK_TCPIP_CORE()        sys_unlock_tcpip_core()
#endif

#else
#define LOCK_TCPIP_CORE()
#define UNLOCK_TCPIP_CORE()
#endif /* LWIP_TCPIP_CORE_LOCKING */

/*
   ------------------------------------
   ---------- Memory options ----------
   ------------------------------------
*/
/**
 * MEM_LIBC_MALLOC==1: Use malloc/free/realloc provided by your C-library
 * instead of the lwip internal allocator. Can save code size if you
 * already use it.
 */
#define MEM_LIBC_MALLOC        1

/**
 * MEMP_MEM_MALLOC==1: Use mem_malloc/mem_free instead of the lwip pool allocator.
 * Especially useful with MEM_LIBC_MALLOC but handle with care regarding execution
 * speed (heap alloc can be much slower than pool alloc) and usage from interrupts
 * (especially if your netif driver allocates PBUF_POOL pbufs for received frames
 * from interrupt)!
 * ATTENTION: Currently, this uses the heap for ALL pools (also for private pools,
 * not only for internal pools defined in memp_std.h)!
 */
#define MEMP_MEM_MALLOC                 1

/**
 * MEM_ALIGNMENT: should be set to the alignment of the CPU
 *    4 byte alignment -> \#define MEM_ALIGNMENT 4
 *    2 byte alignment -> \#define MEM_ALIGNMENT 2
 */
#define MEM_ALIGNMENT           4

/**
 * MEMP_OVERFLOW_CHECK: memp overflow protection reserves a configurable
 * amount of bytes before and after each memp element in every pool and fills
 * it with a prominent default value.
 *    MEMP_OVERFLOW_CHECK == 0 no checking
 *    MEMP_OVERFLOW_CHECK == 1 checks each element when it is freed
 *    MEMP_OVERFLOW_CHECK >= 2 checks each element in every pool every time
 *      memp_malloc() or memp_free() is called (useful but slow!)
 */
#ifndef MEMP_OVERFLOW_CHECK
#define MEMP_OVERFLOW_CHECK             0
#endif

/*
   ------------------------------------------------
   ---------- Internal Memory Pool Sizes ----------
   ------------------------------------------------
*/

/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 * This also sets the number of lwip socket descriptors.
 */
#ifndef MEMP_NUM_NETCONN
#define MEMP_NUM_NETCONN                12
#endif

/*
   --------------------------------
   ---------- ARP options -------
   --------------------------------
*/

/**
 * LWIP_ARP==1: Enable ARP functionality.
 */
#ifndef LWIP_ARP
#define LWIP_ARP                        1
#endif

/**
 * ARP_QUEUEING==1: Multiple outgoing packets are queued during hardware address
 * resolution. By default, only the most recent packet is queued per IP address.
 * This is sufficient for most protocols and mainly reduces TCP connection
 * startup time. Set this to 1 if you know your application sends more than one
 * packet in a row to an IP address that is not in the ARP cache.
 */
#ifndef ARP_QUEUEING
#define ARP_QUEUEING                    1
#endif

/*
   --------------------------------
   ---------- IP options ----------
   --------------------------------
*/
/**
 * IP_REASSEMBLY==1: Reassemble incoming fragmented IP packets. Note that
 * this option does not affect outgoing packet sizes, which can be controlled
 * via IP_FRAG.
 */
#ifndef IP_REASSEMBLY
#define IP_REASSEMBLY                   1
#endif

/**
 * IP_FRAG==1: Fragment outgoing IP packets if their size exceeds MTU. Note
 * that this option does not affect incoming packet sizes, which can be
 * controlled via IP_REASSEMBLY.
 */
#ifndef IP_FRAG
#define IP_FRAG                         1
#endif

/**
 * IP_REASS_MAXAGE: Maximum time (in multiples of IP_TMR_INTERVAL - so seconds, normally)
 * a fragmented IP packet waits for all fragments to arrive. If not all fragments arrived
 * in this time, the whole packet is discarded.
 */
#ifndef IP_REASS_MAXAGE
#define IP_REASS_MAXAGE                 3
#endif

/**
 * IP_REASS_MAX_PBUFS: Total maximum amount of pbufs waiting to be reassembled.
 * Since the received pbufs are enqueued, be sure to configure
 * PBUF_POOL_SIZE > IP_REASS_MAX_PBUFS so that the stack is still able to receive
 * packets even if the maximum amount of fragments is enqueued for reassembly!
 */
#ifndef IP_REASS_MAX_PBUFS
#define IP_REASS_MAX_PBUFS              2
#endif

/*
   ----------------------------------
   ---------- ICMP options ----------
   ----------------------------------
*/

/*
   ---------------------------------
   ---------- RAW options ----------
   ---------------------------------
*/

/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/
/**
 * LWIP_DHCP==1: Enable DHCP module.
 */
#ifndef LWIP_DHCP
#define LWIP_DHCP                       1
#endif

/**
 * DHCP_DOES_ARP_CHECK==1: Do an ARP check on the offered address.
 */
#ifndef DHCP_DOES_ARP_CHECK
#define DHCP_DOES_ARP_CHECK             ((LWIP_DHCP) && (LWIP_ARP))
#endif

#define LWIP_DHCP_BOOTP_FILE            0

/*
   ------------------------------------
   ---------- AUTOIP options ----------
   ------------------------------------
*/
/*
   ----------------------------------
   ----- SNMP MIB2 support      -----
   ----------------------------------
*/
/*
   ----------------------------------
   ----- Multicast/IGMP options -----
   ----------------------------------
*/
/**
 * LWIP_IGMP==1: Turn on IGMP module.
 */
#ifndef LWIP_IGMP
#define LWIP_IGMP                       1
#endif

/*
   ----------------------------------
   ---------- DNS options -----------
   ----------------------------------
*/
/**
 * LWIP_DNS==1: Turn on DNS module. UDP must be available for DNS
 * transport.
 */
#ifndef LWIP_DNS
#define LWIP_DNS                        1
#endif

/** DNS maximum number of entries to maintain locally. */
#ifndef DNS_TABLE_SIZE
#define DNS_TABLE_SIZE 1
#endif

/** DNS maximum host name length supported in the name table. */
#ifndef DNS_MAX_NAME_LENGTH
#define DNS_MAX_NAME_LENGTH 128
#endif

/** Set this to 1 to enable querying ".local" names via mDNS
 *  using a One-Shot Multicast DNS Query */
#ifndef LWIP_DNS_SUPPORT_MDNS_QUERIES
#define LWIP_DNS_SUPPORT_MDNS_QUERIES  1
#endif

/*
   ---------------------------------
   ---------- UDP options ----------
   ---------------------------------
*/
/*
   ---------------------------------
   ---------- TCP options ----------
   ---------------------------------
*/
/**
 * TCP_MAXRTX: Maximum number of retransmissions of data segments.
 */
#ifndef TCP_MAXRTX
#define TCP_MAXRTX                      6
#endif

/**
 * TCP_SYNMAXRTX: Maximum number of retransmissions of SYN segments.
 */
#ifndef TCP_SYNMAXRTX
#define TCP_SYNMAXRTX                   3
#endif

/**
 * TCP_QUEUE_OOSEQ==1: TCP will queue segments that arrive out of order.
 * Define to 0 if your device is low on memory.
 */
#ifndef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ                 1
#endif

/**
 * LWIP_TCP_SACK_OUT==1: TCP will support sending selective acknowledgements (SACKs).
 */
#ifndef LWIP_TCP_SACK_OUT
#define LWIP_TCP_SACK_OUT               1
#endif

/**
 * TCP_MSS: TCP Maximum segment size. (default is 536, a conservative default,
 * you might want to increase this.)
 * For the receive side, this MSS is advertised to the remote side
 * when opening a connection. For the transmit size, this MSS sets
 * an upper limit on the MSS advertised by the remote host.
 */
#ifndef TCP_MSS
#define TCP_MSS                         1460
#endif

/**
 * TCP_OOSEQ_MAX_BYTES: The default maximum number of bytes queued on ooseq per
 * pcb if TCP_OOSEQ_BYTES_LIMIT is not defined. Default is 0 (no limit).
 * Only valid for TCP_QUEUE_OOSEQ==1.
 */
#ifndef TCP_OOSEQ_MAX_BYTES
#define TCP_OOSEQ_MAX_BYTES             (2 * TCP_MSS)
#endif

/**
 * TCP_OOSEQ_BYTES_LIMIT(ooseq): Return the maximum number of bytes to be queued
 * on ooseq per pcb, given the pcb.  Only valid for TCP_QUEUE_OOSEQ==1.
 */
#if !defined TCP_OOSEQ_BYTES_LIMIT
#define TCP_OOSEQ_BYTES_LIMIT(ooseq)    ooseq_bytes_limit(ooseq)
#endif

/**
 * TCP_OOSEQ_MAX_PBUFS: The default maximum number of pbufs queued on ooseq per
 * pcb if TCP_OOSEQ_BYTES_LIMIT is not defined. Default is 0 (no limit).
 * Only valid for TCP_QUEUE_OOSEQ==1.
 */
#ifndef TCP_OOSEQ_MAX_PBUFS
#define TCP_OOSEQ_MAX_PBUFS             2
#endif

/**
 * TCP_OOSEQ_PBUFS_LIMIT(ooseq): Return the maximum number of pbufs to be queued
 * on ooseq per pcb, given the pcb.  Only valid for TCP_QUEUE_OOSEQ==1.
 */
#ifndef TCP_OOSEQ_PBUFS_LIMIT
#define TCP_OOSEQ_PBUFS_LIMIT(ooseq)        ooseq_pbufs_limit(ooseq)
#endif

/**
 * TCP_LISTEN_BACKLOG: Enable the backlog option for tcp listen pcb.
 */
#ifndef TCP_LISTEN_BACKLOG
#define TCP_LISTEN_BACKLOG              1
#endif

/**
 * The maximum allowed backlog for TCP listen netconns.
 * This backlog is used unless another is explicitly specified.
 * 0xff is the maximum (u8_t).
 */
#ifndef TCP_DEFAULT_LISTEN_BACKLOG
#define TCP_DEFAULT_LISTEN_BACKLOG      2
#endif

/**
 * TCP_OVERSIZE: The maximum number of bytes that tcp_write may
 * allocate ahead of time in an attempt to create shorter pbuf chains
 * for transmission. The meaningful range is 0 to TCP_MSS. Some
 * suggested values are:
 *
 * 0:         Disable oversized allocation. Each tcp_write() allocates a new
              pbuf (old behaviour).
 * 1:         Allocate size-aligned pbufs with minimal excess. Use this if your
 *            scatter-gather DMA requires aligned fragments.
 * 128:       Limit the pbuf/memory overhead to 20%.
 * TCP_MSS:   Try to create unfragmented TCP packets.
 * TCP_MSS/4: Try to create 4 fragments or less per TCP packet.
 */
#ifndef TCP_OVERSIZE
#define TCP_OVERSIZE                    TCP_MSS
#endif

/**
 * LWIP_TCP_TIMESTAMPS==1: support the TCP timestamp option.
 * The timestamp option is currently only used to help remote hosts, it is not
 * really used locally. Therefore, it is only enabled when a TS option is
 * received in the initial SYN packet from a remote host.
 */
#ifndef LWIP_TCP_TIMESTAMPS
#define LWIP_TCP_TIMESTAMPS             1
#endif

/**
 * TCP_WND_UPDATE_THRESHOLD: difference in window to trigger an
 * explicit window update
 */
#ifndef TCP_WND_UPDATE_THRESHOLD
#define TCP_WND_UPDATE_THRESHOLD   LWIP_MIN((TCP_WND / 4), (TCP_MSS * 4))
#endif

/*
   ----------------------------------
   ---------- Pbuf options ----------
   ----------------------------------
*/

/**
 * PBUF_LINK_ENCAPSULATION_HLEN: the number of bytes that should be allocated
 * for an additional encapsulation header before ethernet headers (e.g. 802.11)
 *
 * 1. LINK_HLEN 14Byte will be remove in WLAN layer
 * 2. IEEE80211_HDR_MAX_LEN needs 40 bytes.
 * 3. encryption needs exra 4 bytes ahead of actual data payload, and require
 *     DAddr and SAddr to be 4-byte aligned.
 * 4. TRANSPORT and IP are all 20, 4 bytes aligned, nice...
 * 5. LCC add 6 bytes more, We don't consider WAPI yet...
 * 6. define LWIP_MEM_ALIGN to be 4 Byte aligned, pbuf struct is 16B, Only thing may be
 *     matter is ether_hdr is not 4B aligned.
 *
 * So, we need extra (40 + 4 - 14) = 30 and it's happen to be 4-Byte aligned
 *
 *    1. lwip
 *         | empty 30B    | eth_hdr (14B)  | payload ...|
 *              total: 44B ahead payload
 *    2. net80211
 *         | max 80211 hdr, 32B | ccmp/tkip iv (8B) | sec rsv(4B) | payload ...|
 *              total: 40B ahead sec_rsv and 44B ahead payload
 *
 */
#define PBUF_LINK_ENCAPSULATION_HLEN    36

/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/
/**
 * LWIP_NETIF_HOSTNAME==1: use DHCP_OPTION_HOSTNAME with netif's hostname
 * field.
 */
#ifndef LWIP_NETIF_HOSTNAME
#define LWIP_NETIF_HOSTNAME             1
#endif

/**
 * LWIP_NETIF_API==1: Support netif api (in netifapi.c)
 */
#ifndef LWIP_NETIF_API
#define LWIP_NETIF_API                  0
#endif

/**
 * LWIP_NETIF_STATUS_CALLBACK==1: Support a callback function whenever an interface
 * changes its up/down status (i.e., due to DHCP IP acquisition)
 */
#ifndef LWIP_NETIF_STATUS_CALLBACK
#define LWIP_NETIF_STATUS_CALLBACK      0
#endif

/**
 * LWIP_NETIF_TX_SINGLE_PBUF: if this is set to 1, lwIP *tries* to put all data
 * to be sent into one single pbuf. This is for compatibility with DMA-enabled
 * MACs that do not support scatter-gather.
 * Beware that this might involve CPU-memcpy before transmitting that would not
 * be needed without this flag! Use this only if you need to!
 */
#define LWIP_NETIF_TX_SINGLE_PBUF             1

/*
   ------------------------------------
   ---------- LOOPIF options ----------
   ------------------------------------
*/

/*
   ------------------------------------
   ---------- SLIPIF options ----------
   ------------------------------------
*/

/*
   ------------------------------------
   ---------- Thread options ----------
   ------------------------------------
*/
/**
 * TCPIP_THREAD_STACKSIZE: The stack size used by the main tcpip thread.
 * The stack size value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#ifndef TCPIP_THREAD_STACKSIZE
#define TCPIP_THREAD_STACKSIZE          480
#endif

/**
 * TCPIP_THREAD_PRIO: The priority assigned to the main tcpip thread.
 * The priority value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#define TCPIP_THREAD_PRIO               (configMAX_PRIORITIES-5)

/**
 * TCPIP_MBOX_SIZE: The mailbox size for the tcpip thread messages
 * The queue size value itself is platform-dependent, but is passed to
 * sys_mbox_new() when tcpip_init is called.
 */
#define TCPIP_MBOX_SIZE                 16

/**
 * DEFAULT_UDP_RECVMBOX_SIZE: The mailbox size for the incoming packets on a
 * NETCONN_UDP. The queue size value itself is platform-dependent, but is passed
 * to sys_mbox_new() when the recvmbox is created.
 */
#define DEFAULT_UDP_RECVMBOX_SIZE       6

/**
 * DEFAULT_TCP_RECVMBOX_SIZE: The mailbox size for the incoming packets on a
 * NETCONN_TCP. The queue size value itself is platform-dependent, but is passed
 * to sys_mbox_new() when the recvmbox is created.
 */
#define DEFAULT_TCP_RECVMBOX_SIZE       6

/**
 * DEFAULT_ACCEPTMBOX_SIZE: The mailbox size for the incoming connections.
 * The queue size value itself is platform-dependent, but is passed to
 * sys_mbox_new() when the acceptmbox is created.
 */
#define DEFAULT_ACCEPTMBOX_SIZE         6

/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/

/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/

/**
 * LWIP_POSIX_SOCKETS_IO_NAMES==1: Enable POSIX-style sockets functions names.
 * Disable this option if you use a POSIX operating system that uses the same
 * names (read, write & close). (only used if you use sockets.c)
 */
#define LWIP_POSIX_SOCKETS_IO_NAMES     0

/**
 * LWIP_SOCKET_OFFSET==n: Increases the file descriptor number created by LwIP with n.
 * This can be useful when there are multiple APIs which create file descriptors.
 * When they all start with a different offset and you won't make them overlap you can
 * re implement read/write/close/ioctl/fnctl to send the requested action to the right
 * library (sharing select will need more work though).
 */
#ifndef LWIP_SOCKET_OFFSET
#define LWIP_SOCKET_OFFSET              3
#endif

/**
 * LWIP_SO_SNDTIMEO==1: Enable send timeout for sockets/netconns and
 * SO_SNDTIMEO processing.
 */
#ifndef LWIP_SO_SNDTIMEO
#define LWIP_SO_SNDTIMEO                1
#endif

/**
 * LWIP_SO_RCVTIMEO==1: Enable receive timeout for sockets/netconns and
 * SO_RCVTIMEO processing.
 */
#ifndef LWIP_SO_RCVTIMEO
#define LWIP_SO_RCVTIMEO                1
#endif

/**
 * LWIP_TCP_KEEPALIVE==1: Enable TCP_KEEPIDLE, TCP_KEEPINTVL and TCP_KEEPCNT
 * options processing. Note that TCP_KEEPIDLE and TCP_KEEPINTVL have to be set
 * in seconds. (does not require sockets.c, and will affect tcp.c)
 */
#ifndef LWIP_TCP_KEEPALIVE
#define LWIP_TCP_KEEPALIVE              1
#endif

/**
 * LWIP_SO_RCVBUF==1: Enable SO_RCVBUF processing.
 */
#ifndef LWIP_SO_RCVBUF
#define LWIP_SO_RCVBUF                  0
#endif

/**
 * SO_REUSE==1: Enable SO_REUSEADDR option.
 */
#ifndef SO_REUSE
#define SO_REUSE                        1
#endif

/*
   ----------------------------------------
   ---------- Statistics options ----------
   ----------------------------------------
*/

/**
 * LWIP_STATS==1: Enable statistics collection in lwip_stats.
 */
#ifndef LWIP_STATS
#define LWIP_STATS                      0
#endif

/**
 * LWIP_STATS_DISPLAY==1: Compile in the statistics output functions.
 */
#ifndef LWIP_STATS_DISPLAY
#define LWIP_STATS_DISPLAY              0
#endif

/*
   ---------------------------------
   ---------- PPP options ----------
   ---------------------------------
*/

/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

/*
   ---------------------------------------
   ---------- IPv6 options ---------------
   ---------------------------------------
*/
/**
 * LWIP_IPV6==1: Enable IPv6
 */
#ifndef LWIP_IPV6
#define LWIP_IPV6                       0
#endif

/*
   ---------------------------------------
   ---------- Hook options ---------------
   ---------------------------------------
*/

/*
   ---------------------------------------
   ---------- mDNS options ---------------
   ---------------------------------------
*/

/**
 * LWIP_MDNS_RESPONDER_QUEUE_ANNOUNCEMENTS==1: Unsolicited announcements are
 * queued and run from a timer callback.
 */
#ifndef LWIP_MDNS_RESPONDER_QUEUE_ANNOUNCEMENTS
#define LWIP_MDNS_RESPONDER_QUEUE_ANNOUNCEMENTS     1
#endif

/*
   ---------------------------------------
   ---------- Debugging options ----------
   ---------------------------------------
*/

// Uncomment this line, and set the individual debug options you want, for IP stack debug output
//#define LWIP_DEBUG

/**
 * LWIP_DBG_MIN_LEVEL: After masking, the value of the debug is
 * compared against this value. If it is smaller, then debugging
 * messages are written.
 */
//#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_WARNING

/**
 * LWIP_DBG_TYPES_ON: A mask that can be used to globally enable/disable
 * debug messages of certain types.
 */
#define LWIP_DBG_TYPES_ON               LWIP_DBG_ON

/**
 * ETHARP_DEBUG: Enable debugging in etharp.c.
 */
#define ETHARP_DEBUG                    LWIP_DBG_OFF

/**
 * NETIF_DEBUG: Enable debugging in netif.c.
 */
#define NETIF_DEBUG                     LWIP_DBG_OFF

/**
 * PBUF_DEBUG: Enable debugging in pbuf.c.
 */
#define PBUF_DEBUG                      LWIP_DBG_OFF

/**
 * API_LIB_DEBUG: Enable debugging in api_lib.c.
 */
#define API_LIB_DEBUG                   LWIP_DBG_OFF

/**
 * API_MSG_DEBUG: Enable debugging in api_msg.c.
 */
#define API_MSG_DEBUG                   LWIP_DBG_OFF

/**
 * SOCKETS_DEBUG: Enable debugging in sockets.c.
 */
#define SOCKETS_DEBUG                   LWIP_DBG_OFF

/**
 * ICMP_DEBUG: Enable debugging in icmp.c.
 */
#define ICMP_DEBUG                      LWIP_DBG_OFF

/**
 * IGMP_DEBUG: Enable debugging in igmp.c.
 */
#define IGMP_DEBUG                      LWIP_DBG_OFF

/**
 * INET_DEBUG: Enable debugging in inet.c.
 */
#define INET_DEBUG                      LWIP_DBG_OFF

/**
 * IP_DEBUG: Enable debugging for IP.
 */
#define IP_DEBUG                        LWIP_DBG_OFF

/**
 * IP_REASS_DEBUG: Enable debugging in ip_frag.c for both frag & reass.
 */
#define IP_REASS_DEBUG                  LWIP_DBG_OFF

/**
 * RAW_DEBUG: Enable debugging in raw.c.
 */
#define RAW_DEBUG                       LWIP_DBG_OFF

/**
 * MEM_DEBUG: Enable debugging in mem.c.
 */
#define MEM_DEBUG                       LWIP_DBG_OFF

/**
 * MEMP_DEBUG: Enable debugging in memp.c.
 */
#define MEMP_DEBUG                      LWIP_DBG_OFF

/**
 * SYS_DEBUG: Enable debugging in sys.c.
 */
#define SYS_DEBUG                       LWIP_DBG_OFF

/**
 * TIMERS_DEBUG: Enable debugging in timers.c.
 */
#define TIMERS_DEBUG                    LWIP_DBG_OFF

/**
 * TCP_DEBUG: Enable debugging for TCP.
 */
#define TCP_DEBUG                       LWIP_DBG_OFF

/**
 * TCP_INPUT_DEBUG: Enable debugging in tcp_in.c for incoming debug.
 */
#define TCP_INPUT_DEBUG                 LWIP_DBG_OFF

/**
 * TCP_FR_DEBUG: Enable debugging in tcp_in.c for fast retransmit.
 */
#define TCP_FR_DEBUG                    LWIP_DBG_OFF

/**
 * TCP_RTO_DEBUG: Enable debugging in TCP for retransmit
 * timeout.
 */
#define TCP_RTO_DEBUG                   LWIP_DBG_OFF

/**
 * TCP_CWND_DEBUG: Enable debugging for TCP congestion window.
 */
#define TCP_CWND_DEBUG                  LWIP_DBG_OFF

/**
 * TCP_WND_DEBUG: Enable debugging in tcp_in.c for window updating.
 */
#define TCP_WND_DEBUG                   LWIP_DBG_OFF

/**
 * TCP_OUTPUT_DEBUG: Enable debugging in tcp_out.c output functions.
 */
#define TCP_OUTPUT_DEBUG                LWIP_DBG_OFF

/**
 * TCP_RST_DEBUG: Enable debugging for TCP with the RST message.
 */
#define TCP_RST_DEBUG                   LWIP_DBG_OFF

/**
 * TCP_QLEN_DEBUG: Enable debugging for TCP queue lengths.
 */
#define TCP_QLEN_DEBUG                  LWIP_DBG_OFF

/**
 * UDP_DEBUG: Enable debugging in UDP.
 */
#define UDP_DEBUG                       LWIP_DBG_OFF

/**
 * TCPIP_DEBUG: Enable debugging in tcpip.c.
 */
#define TCPIP_DEBUG                     LWIP_DBG_OFF

/**
 * SLIP_DEBUG: Enable debugging in slipif.c.
 */
#define SLIP_DEBUG                      LWIP_DBG_OFF

/**
 * DHCP_DEBUG: Enable debugging in dhcp.c.
 */
#define DHCP_DEBUG                      LWIP_DBG_OFF

/**
 * AUTOIP_DEBUG: Enable debugging in autoip.c.
 */
#define AUTOIP_DEBUG                    LWIP_DBG_OFF

/**
 * DNS_DEBUG: Enable debugging for DNS.
 */
#define DNS_DEBUG                       LWIP_DBG_OFF

/**
 * IP6_DEBUG: Enable debugging for IPv6.
 */
#define IP6_DEBUG                       LWIP_DBG_OFF

/**
 * MDNS_DEBUG: Enable debugging for multicast DNS.
 */
#define MDNS_DEBUG                      LWIP_DBG_OFF

/*
   --------------------------------------------------
   ---------- Performance tracking options ----------
   --------------------------------------------------
*/

#endif /* __LWIPOPTS_H__ */

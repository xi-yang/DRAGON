#ifndef _RSVP_config_h_
#define _RSVP_config_h_ 1

/* size of label hash container for MPLS */
#define LABEL_HASH_COUNT 65536

/* size of message id hash container for refresh reduction */
#define MESSAGE_ID_HASH_COUNT_SEND 131072

/* size of message id hash container for refresh reduction */
#define MESSAGE_ID_HASH_COUNT_RECV 131072

/* size of api hash container -> useful for large tests with few end systems */
#define API_HASH_COUNT 1

/* size of session hash container */
#define SESSION_HASH_COUNT 4096

/* define to include dest port number into hash calculation
   note: this is useful for certain tests, but prohibits 100%-correct RSVP */
#undef SESSION_HASH_PORTS

/* default total period which is covered by timer wheel */
#define TIMER_SLOT_TOTAL_PERIOD 600

#define MPLS_REAL	(defined(ENABLE_MPLS) && (defined(MPLS_WISCONSIN) || defined(MPLS_CAMBRIDGE)))

#undef ALTQ_DEVICE
#undef CBQ_DEVICE
#undef ENABLE_ALTQ
#undef ENABLE_CBQ
#undef ENABLE_MPLS
#undef FIXED_TIMEOUTS
#undef FUZZY_TIMERS
#undef GETSOCKOPT_SIZE_T
#undef GETSOCKNAME_SIZE_T
#undef HAVE_KLD
#undef HAVE_SIN_LEN
#undef HFSC_DEVICE
#undef HTONS_IP_HEADER
#undef INVERSE_DNS
#undef MPLS_WISCONSIN
#undef MPLS_CAMBRIDGE
#undef NEED_IN_PKTINFO
#undef NEED_MULTICAST_TTL
#undef NEED_RA_SOCKOPT
#undef NEED_UNICAST_TTL
#undef NO_TIMERS
#undef ONEPASS_RESERVATION
#undef PIDFILE
#undef REAL_NETWORK
#undef SO_REUSEXXX
#undef REFRESH_REDUCTION
#undef RSRR_SERV_PATH
#undef RSRR_CLI_PATH
#undef RSVP_MEMORY_MACHINE
#undef RSVP_STATS
#undef RECVFROM_BUF_T
#undef RECVFROM_SIZE_T
#undef RSVP_CHECKS
#undef SENDTO_BUF_T
#undef STAMP_DEVICE
#undef USE_SCOPE_OBJECT
#undef VIRT_NETWORK
#undef WITH_API
#undef WITH_JAVA_API

#undef Linux
#undef FreeBSD
#undef SunOS

#undef FATAL_ON
#undef ERROR_ON
#undef CHECK_ON
#undef LOG_ON
#undef NDEBUG

#if !defined(REAL_NETWORK) && !defined(VIRT_NETWORK)
#define NS2	1
#endif

@TOP@

@BOTTOM@

#endif /* _RSVP_config_h_ */

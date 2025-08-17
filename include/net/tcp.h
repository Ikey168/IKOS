/* IKOS Network Stack - TCP Protocol Implementation
 * Issue #44 - TCP/IP Protocol Implementation
 * 
 * Implements TCP (Transmission Control Protocol) for reliable connection-oriented communication
 * RFC 793 - Transmission Control Protocol
 */

#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include <stdbool.h>
#include "network.h"
#include "ip.h"
#include "socket.h"

/* ================================
 * TCP Constants
 * ================================ */

#define TCP_HEADER_MIN_SIZE    20      /* Minimum TCP header size */
#define TCP_HEADER_MAX_SIZE    60      /* Maximum TCP header size (with options) */
#define TCP_MAX_SEGMENT_SIZE   1460    /* Maximum segment size (1500 - IP - TCP headers) */
#define TCP_MIN_MSS            536     /* Minimum MSS to advertise */
#define TCP_DEFAULT_MSS        1460    /* Default MSS */
#define TCP_MAX_WINDOW         65535   /* Maximum window size */
#define TCP_INITIAL_WINDOW     8192    /* Initial receive window size */

/* TCP Port ranges */
#define TCP_MIN_PORT          1        /* Minimum port number */
#define TCP_MAX_PORT          65535    /* Maximum port number */
#define TCP_EPHEMERAL_MIN     49152    /* Start of ephemeral port range */
#define TCP_EPHEMERAL_MAX     65535    /* End of ephemeral port range */

/* TCP Timeouts (in milliseconds) */
#define TCP_INITIAL_RTO       3000     /* Initial retransmission timeout */
#define TCP_MIN_RTO           200      /* Minimum RTO */
#define TCP_MAX_RTO           60000    /* Maximum RTO */
#define TCP_KEEPALIVE_TIME    7200000  /* Keep-alive time (2 hours) */
#define TCP_KEEPALIVE_INTVL   75000    /* Keep-alive interval */
#define TCP_KEEPALIVE_PROBES  9        /* Keep-alive probe count */
#define TCP_TIME_WAIT_TIMEOUT 120000   /* TIME_WAIT timeout (2MSL) */

/* TCP Congestion Control */
#define TCP_INITIAL_CWND      4        /* Initial congestion window (segments) */
#define TCP_SSTHRESH_INITIAL  65535    /* Initial slow start threshold */

/* Well-known TCP ports */
#define TCP_PORT_FTP_DATA     20       /* FTP Data */
#define TCP_PORT_FTP_CONTROL  21       /* FTP Control */
#define TCP_PORT_SSH          22       /* SSH */
#define TCP_PORT_TELNET       23       /* Telnet */
#define TCP_PORT_SMTP         25       /* SMTP */
#define TCP_PORT_HTTP         80       /* HTTP */
#define TCP_PORT_POP3         110      /* POP3 */
#define TCP_PORT_IMAP         143      /* IMAP */
#define TCP_PORT_HTTPS        443      /* HTTPS */

/* ================================
 * TCP Header Structure
 * ================================ */

/* TCP Header Flags */
#define TCP_FLAG_FIN          0x01     /* Finish - no more data */
#define TCP_FLAG_SYN          0x02     /* Synchronize sequence numbers */
#define TCP_FLAG_RST          0x04     /* Reset connection */
#define TCP_FLAG_PSH          0x08     /* Push data */
#define TCP_FLAG_ACK          0x10     /* Acknowledgment field valid */
#define TCP_FLAG_URG          0x20     /* Urgent pointer valid */
#define TCP_FLAG_ECE          0x40     /* ECN Echo */
#define TCP_FLAG_CWR          0x80     /* Congestion Window Reduced */

/* TCP Header (RFC 793) */
typedef struct __attribute__((packed)) {
    uint16_t src_port;         /* Source port */
    uint16_t dest_port;        /* Destination port */
    uint32_t seq_num;          /* Sequence number */
    uint32_t ack_num;          /* Acknowledgment number */
    uint8_t data_offset : 4;   /* Data offset (header length in 32-bit words) */
    uint8_t reserved : 4;      /* Reserved (must be zero) */
    uint8_t flags;             /* Control flags */
    uint16_t window_size;      /* Window size */
    uint16_t checksum;         /* Checksum */
    uint16_t urgent_ptr;       /* Urgent pointer */
    uint8_t options[];         /* Options (variable length) */
} tcp_header_t;

/* ================================
 * TCP Connection States
 * ================================ */

typedef enum {
    TCP_CLOSED = 0,            /* Connection closed */
    TCP_LISTEN,                /* Listening for connections */
    TCP_SYN_SENT,              /* SYN sent, waiting for SYN-ACK */
    TCP_SYN_RCVD,              /* SYN received, waiting for ACK */
    TCP_ESTABLISHED,           /* Connection established */
    TCP_FIN_WAIT_1,            /* FIN sent, waiting for ACK */
    TCP_FIN_WAIT_2,            /* FIN ACKed, waiting for FIN */
    TCP_CLOSE_WAIT,            /* Received FIN, waiting for close */
    TCP_CLOSING,               /* Simultaneous close */
    TCP_LAST_ACK,              /* Waiting for final ACK */
    TCP_TIME_WAIT,             /* Waiting for network to clear */
    TCP_MAX_STATES
} tcp_state_t;

/* ================================
 * TCP Connection Control Block
 * ================================ */

/* TCP Sequence Space */
typedef struct {
    uint32_t una;              /* Oldest unacknowledged sequence number */
    uint32_t nxt;              /* Next sequence number to send */
    uint32_t wnd;              /* Send window size */
    uint32_t up;               /* Send urgent pointer */
    uint32_t wl1;              /* Sequence number for last window update */
    uint32_t wl2;              /* Acknowledgment for last window update */
    uint32_t iss;              /* Initial send sequence number */
} tcp_send_t;

typedef struct {
    uint32_t nxt;              /* Next sequence number expected */
    uint32_t wnd;              /* Receive window size */
    uint32_t up;               /* Receive urgent pointer */
    uint32_t irs;              /* Initial receive sequence number */
} tcp_recv_t;

/* TCP Connection Control Block */
typedef struct tcp_socket {
    /* Socket identification */
    uint16_t local_port;       /* Local port number */
    uint16_t remote_port;      /* Remote port number */
    ip_addr_t local_addr;      /* Local IP address */
    ip_addr_t remote_addr;     /* Remote IP address */
    
    /* Connection state */
    tcp_state_t state;         /* Current connection state */
    
    /* Sequence number management */
    tcp_send_t snd;            /* Send sequence space */
    tcp_recv_t rcv;            /* Receive sequence space */
    
    /* Window management */
    uint16_t mss;              /* Maximum segment size */
    uint16_t snd_wnd;          /* Send window size */
    uint16_t rcv_wnd;          /* Receive window size */
    uint16_t adv_wnd;          /* Advertised window size */
    
    /* Congestion control */
    uint32_t cwnd;             /* Congestion window */
    uint32_t ssthresh;         /* Slow start threshold */
    uint32_t cwnd_count;       /* Congestion window counter */
    
    /* Retransmission */
    uint32_t rto;              /* Retransmission timeout */
    uint32_t srtt;             /* Smoothed round-trip time */
    uint32_t rttvar;           /* Round-trip time variation */
    uint32_t backoff;          /* Exponential backoff counter */
    
    /* Timers */
    uint32_t retrans_timer;    /* Retransmission timer */
    uint32_t keepalive_timer;  /* Keep-alive timer */
    uint32_t timewait_timer;   /* TIME_WAIT timer */
    
    /* Buffers */
    netbuf_t* send_buffer;     /* Send buffer */
    netbuf_t* recv_buffer;     /* Receive buffer */
    netbuf_t* retrans_queue;   /* Retransmission queue */
    netbuf_t* ooo_queue;       /* Out-of-order queue */
    
    /* Socket options */
    bool nodelay;              /* Disable Nagle's algorithm */
    bool keepalive;            /* Enable keep-alive */
    uint32_t user_timeout;     /* User timeout */
    
    /* Statistics */
    uint64_t packets_sent;     /* Packets transmitted */
    uint64_t packets_received; /* Packets received */
    uint64_t bytes_sent;       /* Bytes transmitted */
    uint64_t bytes_received;   /* Bytes received */
    uint64_t retrans_count;    /* Retransmission count */
    uint64_t duplicate_acks;   /* Duplicate ACK count */
    
    /* List linkage */
    struct tcp_socket* next;   /* Next socket in hash chain */
    struct tcp_socket* parent; /* Parent listening socket */
} tcp_socket_t;

/* ================================
 * TCP Statistics
 * ================================ */

typedef struct {
    /* Connection statistics */
    uint64_t active_opens;     /* Active connection attempts */
    uint64_t passive_opens;    /* Passive connection attempts */
    uint64_t failed_attempts;  /* Failed connection attempts */
    uint64_t established_resets; /* Connections reset after establishment */
    uint64_t current_established; /* Currently established connections */
    
    /* Segment statistics */
    uint64_t segments_sent;    /* Total segments sent */
    uint64_t segments_received; /* Total segments received */
    uint64_t bad_segments;     /* Segments received with errors */
    uint64_t reset_segments;   /* Reset segments sent */
    
    /* Retransmission statistics */
    uint64_t retrans_segments; /* Retransmitted segments */
    uint64_t retrans_timeouts; /* Retransmission timeouts */
    uint64_t fast_retrans;     /* Fast retransmissions */
    
    /* Error statistics */
    uint64_t checksum_errors;  /* Checksum errors */
    uint64_t invalid_segments; /* Invalid segments */
    uint64_t out_of_window;    /* Out-of-window segments */
} tcp_stats_t;

/* ================================
 * TCP Function Declarations
 * ================================ */

/* Protocol Initialization */
int tcp_init(void);
void tcp_shutdown(void);

/* Packet Processing */
int tcp_receive_packet(netdev_t* dev, netbuf_t* buf);
int tcp_send_packet(tcp_socket_t* sock, uint8_t flags, const void* data, size_t len);

/* Socket Operations */
tcp_socket_t* tcp_socket_create(void);
int tcp_socket_bind(tcp_socket_t* sock, ip_addr_t addr, uint16_t port);
int tcp_socket_listen(tcp_socket_t* sock, int backlog);
tcp_socket_t* tcp_socket_accept(tcp_socket_t* sock);
int tcp_socket_connect(tcp_socket_t* sock, ip_addr_t addr, uint16_t port);
int tcp_socket_send(tcp_socket_t* sock, const void* data, size_t len);
int tcp_socket_recv(tcp_socket_t* sock, void* buffer, size_t len);
int tcp_socket_shutdown(tcp_socket_t* sock, int how);
int tcp_socket_close(tcp_socket_t* sock);

/* Connection Management */
int tcp_handle_syn(tcp_socket_t* sock, tcp_header_t* header, netbuf_t* buf);
int tcp_handle_ack(tcp_socket_t* sock, tcp_header_t* header, netbuf_t* buf);
int tcp_handle_fin(tcp_socket_t* sock, tcp_header_t* header, netbuf_t* buf);
int tcp_handle_rst(tcp_socket_t* sock, tcp_header_t* header, netbuf_t* buf);

/* State Machine */
int tcp_state_machine(tcp_socket_t* sock, tcp_header_t* header, netbuf_t* buf);
void tcp_set_state(tcp_socket_t* sock, tcp_state_t new_state);
const char* tcp_state_name(tcp_state_t state);

/* Socket Management */
tcp_socket_t* tcp_find_socket(ip_addr_t local_addr, uint16_t local_port,
                             ip_addr_t remote_addr, uint16_t remote_port);
tcp_socket_t* tcp_find_listening_socket(uint16_t port);
int tcp_register_socket(tcp_socket_t* sock);
int tcp_unregister_socket(tcp_socket_t* sock);

/* Port Management */
uint16_t tcp_allocate_port(void);
int tcp_bind_port(uint16_t port, tcp_socket_t* sock);
int tcp_release_port(uint16_t port);
bool tcp_port_in_use(uint16_t port);

/* Header Operations */
tcp_header_t* tcp_get_header(netbuf_t* buf);
int tcp_build_header(netbuf_t* buf, uint16_t src_port, uint16_t dest_port,
                    uint32_t seq, uint32_t ack, uint8_t flags, uint16_t window);
uint16_t tcp_calculate_checksum(const tcp_header_t* header, ip_addr_t src_addr,
                               ip_addr_t dest_addr, size_t len);
bool tcp_verify_checksum(const tcp_header_t* header, ip_addr_t src_addr,
                        ip_addr_t dest_addr, size_t len);

/* Sequence Number Operations */
bool tcp_seq_between(uint32_t seq, uint32_t start, uint32_t end);
bool tcp_seq_gt(uint32_t seq1, uint32_t seq2);
bool tcp_seq_ge(uint32_t seq1, uint32_t seq2);
bool tcp_seq_lt(uint32_t seq1, uint32_t seq2);
bool tcp_seq_le(uint32_t seq1, uint32_t seq2);

/* Window Management */
int tcp_update_window(tcp_socket_t* sock, tcp_header_t* header);
uint16_t tcp_calculate_window(tcp_socket_t* sock);
int tcp_probe_zero_window(tcp_socket_t* sock);

/* Congestion Control */
void tcp_init_congestion_control(tcp_socket_t* sock);
void tcp_slow_start(tcp_socket_t* sock);
void tcp_congestion_avoidance(tcp_socket_t* sock);
void tcp_fast_retransmit(tcp_socket_t* sock);
void tcp_fast_recovery(tcp_socket_t* sock);

/* Retransmission */
void tcp_calculate_rto(tcp_socket_t* sock, uint32_t rtt);
int tcp_retransmit_segment(tcp_socket_t* sock);
void tcp_reset_retransmission_timer(tcp_socket_t* sock);

/* Timer Management */
void tcp_timer_tick(void);
void tcp_start_timer(tcp_socket_t* sock, int timer_type, uint32_t timeout);
void tcp_stop_timer(tcp_socket_t* sock, int timer_type);

/* Options Processing */
int tcp_process_options(tcp_socket_t* sock, tcp_header_t* header);
int tcp_build_options(tcp_socket_t* sock, uint8_t* options, size_t max_len);

/* Utility Functions */
void tcp_print_header(const tcp_header_t* header);
void tcp_dump_socket(const tcp_socket_t* sock);
void tcp_print_stats(void);
void tcp_reset_stats(void);
tcp_stats_t* tcp_get_stats(void);

/* ================================
 * TCP Error Codes
 * ================================ */

#define TCP_SUCCESS            0       /* Operation successful */
#define TCP_ERROR_INVALID_ARG  -1      /* Invalid argument */
#define TCP_ERROR_NO_MEMORY    -2      /* Out of memory */
#define TCP_ERROR_PORT_IN_USE  -3      /* Port already in use */
#define TCP_ERROR_NO_SOCKET    -4      /* No matching socket found */
#define TCP_ERROR_CONN_REFUSED -5      /* Connection refused */
#define TCP_ERROR_CONN_RESET   -6      /* Connection reset */
#define TCP_ERROR_CONN_TIMEOUT -7      /* Connection timed out */
#define TCP_ERROR_NOT_CONNECTED -8     /* Socket not connected */
#define TCP_ERROR_ALREADY_CONNECTED -9 /* Socket already connected */
#define TCP_ERROR_INVALID_STATE -10    /* Invalid socket state */
#define TCP_ERROR_BUFFER_FULL   -11    /* Buffer full */
#define TCP_ERROR_WOULD_BLOCK   -12    /* Operation would block */

/* ================================
 * TCP Timer Types
 * ================================ */

#define TCP_TIMER_RETRANS      0       /* Retransmission timer */
#define TCP_TIMER_KEEPALIVE    1       /* Keep-alive timer */
#define TCP_TIMER_TIME_WAIT    2       /* TIME_WAIT timer */

/* ================================
 * TCP Inline Helper Functions
 * ================================ */

/* Get TCP header length in bytes */
static inline uint8_t tcp_get_header_length(const tcp_header_t* header) {
    return (header->data_offset) * 4;
}

/* Check if ACK flag is set */
static inline bool tcp_is_ack(const tcp_header_t* header) {
    return (header->flags & TCP_FLAG_ACK) != 0;
}

/* Check if SYN flag is set */
static inline bool tcp_is_syn(const tcp_header_t* header) {
    return (header->flags & TCP_FLAG_SYN) != 0;
}

/* Check if FIN flag is set */
static inline bool tcp_is_fin(const tcp_header_t* header) {
    return (header->flags & TCP_FLAG_FIN) != 0;
}

/* Check if RST flag is set */
static inline bool tcp_is_rst(const tcp_header_t* header) {
    return (header->flags & TCP_FLAG_RST) != 0;
}

/* Convert network byte order to host byte order */
static inline uint32_t tcp_ntohl(uint32_t net_val) {
    return ntohl(net_val);
}

/* Convert host byte order to network byte order */
static inline uint32_t tcp_htonl(uint32_t host_val) {
    return htonl(host_val);
}

#endif /* TCP_H */

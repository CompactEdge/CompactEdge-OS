#include <stdint.h>
#include <stdbool.h>

struct pktgen_socks {
    int udp_sock;
};

struct pktgen_remaddrs {
    struct sockaddr_in udp_remaddr;
};

struct pktgen_timeval {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct pktgen_msg {
    int64_t xdp_recv;
    int64_t tc_ingress;
    int64_t iptables_out;
    int64_t xdp_cont_recv;
};

enum pktgen_pkt_type {
    rtt_measure_xdp = 0,
    rtt_measure_ipt,
    rtt_measure_cont,
    rtt_latency
};

// Common functions
ssize_t receive_pkt_msg(int sock_fd, struct pktgen_msg *msg, struct timespec *tstamp,
        struct sockaddr_in *remaddr, bool client);

struct pktgen_socks *pktgen_server_init(int port);
struct pktgen_socks *pktgen_client_init(char* interface, int port, char *file, FILE **fp);
int pktgen_server_socket_init(int port, int *sock);
int send_udp_datagram(struct pktgen_socks *socks, struct sockaddr_in *remaddr, int sleep_ms);
int client_warmup(struct pktgen_socks *socks, struct pktgen_msg *msg, struct timespec *tstamp,
        struct sockaddr_in *remaddr, int warmup_cnt);
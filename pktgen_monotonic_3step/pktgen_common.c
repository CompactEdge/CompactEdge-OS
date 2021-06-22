#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <poll.h>
#include <time.h>
#include <linux/errqueue.h>
#include "pktgen.h"


int get_if_address(int fd, char *ifname, struct in_addr *addr)
{
    int ret;
    struct ifreq ifr;
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;
    if ((ret = ioctl(fd, SIOCGIFADDR, &ifr)) < 0) {
        perror("SIOCGIFADDR");
        return ret;
    }
    *addr = sin->sin_addr;
    return 0;
}

int enable_hw_timestamp(int fd, char *ifname)
{
    int ret;
    struct hwtstamp_config config = {.flags = 0};
    struct ifreq ifr;
    unsigned int opt;
    int enabled = 1;
    // Fix from HWTSTAMP_FILTER_ALL to HWTSTAMP_FILTER_PTP_V2_L4_EVENT
    config.tx_type = HWTSTAMP_TX_OFF;
    config.rx_filter = HWTSTAMP_FILTER_ALL;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;
    ifr.ifr_data = (void *)&config;

    if((ret = ioctl(fd, SIOCSHWTSTAMP, &ifr)) < 0)
    {
        perror("SIOCSHWTSTAMP");
        return ret;
    }

    opt = SOF_TIMESTAMPING_RX_HARDWARE |
        SOF_TIMESTAMPING_RAW_HARDWARE |
        SOF_TIMESTAMPING_OPT_CMSG;
    if((ret = setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, (char *)&opt, sizeof(opt))) < 0)
    {
        perror("SO_TIMESTAMPING");
        return ret;
    }
    
    if ((ret = setsockopt(fd, SOL_SOCKET, SO_SELECT_ERR_QUEUE, &enabled,
                    sizeof(enabled))) < 0) {
        perror("SO_SELECT_ERR_QUEUE");
        return ret;
    }
    if((ret = setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &enabled, sizeof(enabled))) < 0)
    {
        perror("IP_PKTINFO");
        return ret;
    }

    return 0;
}

inline ssize_t send_pkt_common(int fd_sock, struct sockaddr_in *remaddr, struct iovec *iov)
{
    struct msghdr m = {0};
    ssize_t size;

    m.msg_name = remaddr;
    m.msg_namelen = sizeof(struct sockaddr_in);
    m.msg_iov = iov;
    m.msg_iovlen = 1;

    errno = 0;
    if ((size = sendmsg(fd_sock, &m, 0)) <= 0) {
        if (errno == EAGAIN) {
            fprintf(stderr, "sendmsg: Request timed out.\n");
            return size;
        }
        perror("sendmsg");
        return size;
    }
    return size;
}

int bind_socket(int sock, char *interface, int port)
{
    int ret;
    struct sockaddr_in myaddr = {0};
    if(interface == NULL) { // server
        myaddr.sin_port = htons(port);
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = INADDR_ANY;

        if((ret = bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr))) < 0) {
            perror("server_init:cont_bind");
            return ret;
        }
        return 0;    
    }

    if((ret = get_if_address(sock, interface, &myaddr.sin_addr)) < 0) {
        perror("client_init:get_if_address");
        return ret;
    
    }
    myaddr.sin_port = htons(port);
    myaddr.sin_family = AF_INET;
    // client bind
    if((ret = bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr))) < 0) {
        perror("client_init:sock_bind");
        return ret;
    }

    return 0;
}

int parse_control_msg(struct msghdr *m, struct timespec *tstamp, int *stamp_found)
{
    struct cmsghdr *cm;
    struct timespec *received_stamp;
    struct sock_extended_err *exterr;

    *stamp_found = 0;
    for ( cm = CMSG_FIRSTHDR(m); cm; cm = CMSG_NXTHDR(m, cm))
    {
        switch (cm->cmsg_level) {
            case SOL_SOCKET:
                switch (cm->cmsg_type) {
                    case SO_TIMESTAMPING:
                        received_stamp = (struct timespec *)CMSG_DATA(cm);
                        *tstamp = received_stamp[2];
                        *stamp_found = 1;
                        break;
                    default:
                        fprintf(stderr, "Unexpected cmsg. level:SOL_SOCKET type:%d\n", cm->cmsg_type);
                        return -1;
                }
                break;
            case IPPROTO_IP:
                switch (cm->cmsg_type) {
                    case IP_RECVERR:
                        exterr = (struct sock_extended_err *)CMSG_DATA(cm);
                        if(!(exterr->ee_errno == ENOMSG &&
                                exterr->ee_origin == SO_EE_ORIGIN_TIMESTAMPING))
                            fprintf(stderr, "Unexpected recverr errno '%s', origin %d\n",
                                strerror(exterr->ee_errno), exterr->ee_origin);
                        break;
                    case IP_PKTINFO:
                        break;
                    default:
                        fprintf(stderr, "Unexpected cmsg level: IPPROTO_IP, type:%d\n",
                            cm->cmsg_type);
                        return -1;
                }
                break;
            default:
                fprintf(stderr, "Unexpected cmsg level:%d, type:%d\n",
                    cm->cmsg_level, cm->cmsg_type);
                return -1;
        }
    }
    return 0;
}

ssize_t receive_pkt_common(int sock_fd, struct iovec *iov, struct timespec *tstamp,
        struct sockaddr_in *remaddr, bool client)
{
    struct msghdr m = {0};
    char ctrlbuf[1024];
    ssize_t size;
//    int ret;
//    int stamp_found;

    m.msg_iov = iov;
    m.msg_iovlen = 1;
    m.msg_name = remaddr;
    m.msg_namelen = sizeof(struct sockaddr_in);
    m.msg_control = ctrlbuf; //TODO: check this
    m.msg_controllen = sizeof(ctrlbuf);

    errno = 0;
    if ((size = recvmsg(sock_fd, &m, 0)) < 0) {
        if (errno == EAGAIN) {
            fprintf(stderr, "recvmsg: Request timed out.\n");
            return size;
        }
        perror("recvmsg");
        return size;
    }
    if(!client) return 0; // server do not get rx timestamp
// HW Tstamp Disabled
//    if ((ret = parse_control_msg(&m, tstamp, &stamp_found)) < 0) return ret;
    
    return size;
}

ssize_t receive_pkt_msg(int sock_fd, struct pktgen_msg *msg, struct timespec *tstamp,
        struct sockaddr_in *remaddr, bool client)
{
    struct iovec iov = {msg, sizeof(*msg)};
    ssize_t size = receive_pkt_common(sock_fd, &iov, tstamp, remaddr, client);

    return size;
}



struct pktgen_socks *pktgen_server_init(int port)
{
    fprintf(stdout, "debug:pktgen_server_init start\n");
    struct pktgen_socks *socks = (struct pktgen_socks *)calloc(1, sizeof(*socks));
    assert(socks);
    
    int ret;
    if((ret = pktgen_server_socket_init(port, &(socks->udp_sock))) < 0) {
        perror("udp_sock.init");
        return NULL;
    }

    if((ret = bind_socket(socks->udp_sock, NULL, port)) < 0) {
        perror("udp_socket.bind");
        return NULL;
    }

    return socks;    
}

int pktgen_server_socket_init(int port, int *sock)
{
 //   int ret;

    //socket
    if ((*sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket.dgram");
        return -1;
    }

    // timeout
//    if((ret = setsockopt(*sock, SOL_SOCKET, SO_SNDTIMEO, NULL, sizeof(struct timeval) )) < 0) {
//        perror("setsockopt.SO_SNDTIMEO");
//        return -1;
//    }

    return 0;
}

struct pktgen_socks *pktgen_client_init(char* interface, int port, char *file, FILE **fp)
{
    struct pktgen_socks *socks = (struct pktgen_socks *)calloc(1, sizeof(*socks));
    int ret;

    assert(socks);

    //socket
    if ((socks->udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket.dgram");
        return NULL;
    }

    // bind to device
    if ((ret = setsockopt(socks->udp_sock, SOL_SOCKET, SO_BINDTODEVICE, interface,
                    strlen(interface)+1)) < 0) {
        perror("setsockopt.SO_BINDTODEVICE");
        return NULL;
    }

    // timeout
//    if((ret = setsockopt(socks->udp_sock, SOL_SOCKET, SO_SNDTIMEO, NULL, 0)) < 0) {
//        perror("setsockopt.SO_SNDTIMEO");
//        return NULL;
//    }


    // bind
    if((ret = bind_socket(socks->udp_sock, interface, port)) < 0) {
        return NULL;
    }

    // enable hw timestamping
//    if((ret = enable_hw_timestamp(socks->udp_sock, interface)) < 0) {
//       perror("enable_hw_timestamp_sock");
//       return NULL;
//   }

    // log file init
    *fp = fopen(file, "w");
    if(!(*fp)) {
        perror("pktgen_client_init.fopen");
        return NULL;
    }
    return socks;
}


int send_udp_datagram(struct pktgen_socks *socks, struct sockaddr_in *remaddr, int sleep_us)
{
    int ret;
    struct pktgen_msg m = {0,};
    struct timespec ts_sleep = {0,};
    struct iovec iov = {&m, sizeof(m)};

    while(1)
    {
        if((ret = send_pkt_common(socks->udp_sock, remaddr, &iov)) < 0) {
            fprintf(stderr, "send_pkt_common: %s\n", gai_strerror(ret));
            return -1;
        }

        // delay per packet
        ts_sleep.tv_sec = 0;
        ts_sleep.tv_nsec = sleep_us*1000;
        do {
            ret = nanosleep(&ts_sleep, &ts_sleep);
        } while (ret && errno == EINTR);

    }

}

int client_warmup(struct pktgen_socks *socks, struct pktgen_msg *msg, struct timespec *tstamp,
        struct sockaddr_in *remaddr, int warmup_cnt)
{
    int ret = 0;
    while(warmup_cnt)
    {
        if((ret = receive_pkt_msg(socks->udp_sock, msg, tstamp, remaddr, true)) < 0) {
            fprintf(stderr, "client.receive_pkt_msg: invalid packet.\n");
            return EXIT_FAILURE;
        }
        warmup_cnt--;
    }
    return 0;

}

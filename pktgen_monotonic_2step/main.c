#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>

#include "pktgen.h"

enum rtt_pktgen_mode {
    mode_none = 0,
    mode_server,
    mode_client
};

static struct option longopts[] ={
    {"interface", required_argument, NULL, 'i'},
    {"delay", required_argument, NULL, 'd'},
    {"addr", required_argument, NULL, 'a'},
    {"port", required_argument, NULL, 'p'},
    {"out_file", required_argument, NULL, 'o'},
    {"sample", required_argument, NULL, 's'},
    {"help" ,no_argument, NULL, 'h'},
    {0, 0, 0, 0}
};


static void usage(void)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, " client: pktgen_edge --client --interface <nic_name> --port [int] --out_file [file]\n");
    fprintf(stderr, " server: pktgen_edge --server --addr <client_ipv4_addr> --delay [us] --port [int] \n\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "--server / --client: set running mode to server/client. server is sends packet, client is receive packet.\n");
    fprintf(stderr, "--interface: CLIENT ONLY, set binding network interface. datagrms are pass to specified NIC.\n");
    fprintf(stderr, "--addr: SERVER ONLY, client IPv4 address.\n");
    fprintf(stderr, "--delay: SERVER ONLY, set delay per entire rounds, us. Deafult: 0\n");
    fprintf(stderr, "--port: port for UDP Datagram\n");
    fprintf(stderr, "--out_file: CLIENT ONLY, specify full file path of output file. latency data will be written in file.\n");
    fprintf(stderr, "--sample: CLIENT ONLY, number of sample data. -1 is inf, default: -1\n\n");
}

int pktgen_server_run(struct pktgen_socks *socks, char* host, char* port, int delay)
{
    struct sockaddr_in remaddr = {0}; 
    struct addrinfo *reminfo = NULL;

    int ret;
    fprintf(stdout, "debug:pktgen_server_run start\n");
    if((ret = getaddrinfo(host, port, NULL, &reminfo)) < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return EXIT_FAILURE;
    }
    memcpy(&remaddr, reminfo->ai_addr, sizeof(remaddr));
    freeaddrinfo(reminfo);

    // Go go go!
    if((ret = send_udp_datagram(socks, &remaddr, delay) < 0)) {
        fprintf(stderr, "send_udp_datagram: error is occured.\n");
        return EXIT_FAILURE;
    }
    return 0;
}

int pktgen_client_run(struct pktgen_socks *socks, char* interface, int64_t sample, FILE *fp)
{
    int ret;
    struct pktgen_msg msg = {0};
    struct sockaddr_in remaddr = {0};
    struct timespec rx_timestamp = {0};
    struct timespec curr_timestamp = {0};
    int64_t diff = 0;    

    while(sample)
    {
        if(sample > 0) sample--;
        if((ret = receive_pkt_msg(socks->udp_sock, &msg, &rx_timestamp, &remaddr, true)) < 0) {
            fprintf(stderr, "client.receive_pkt_msg: invalid packet.\n");
            return EXIT_FAILURE;
        }
        // Get current time
        clock_gettime(CLOCK_MONOTONIC, &curr_timestamp);
        // Get diff
        diff = curr_timestamp.tv_sec * 1000000000UL + curr_timestamp.tv_nsec - msg.xdp_recv;
        // Get 
        // Write to file
        fprintf(fp, "Current Time: %"PRId64"\n", curr_timestamp.tv_sec * 1000000000UL + curr_timestamp.tv_nsec);
        fprintf(fp, "XDP RX: %"PRId64"\n", msg.xdp_recv);
        fprintf(fp, "Scheduling latency (Device to App): %"PRId64"\n\n", diff);
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *fp;
    struct pktgen_socks *socks = NULL;
    enum rtt_pktgen_mode mode = mode_none;
    char *interface = NULL;
    int delay = 0;
    char *client_addr = NULL;
    char *port = NULL;
    char *file = NULL;
    int c, nargc = argc;
    int64_t sample = -1;

    int ret = 0;

    // Disable stdout buffer
    setvbuf(stdout, NULL, _IONBF, 0); 

    if(argc >=2) {
        if(!strcmp(argv[1], "--server")) {
            mode = mode_server;
            nargc--;
        }
        else if(!strcmp(argv[1], "--client")) {
            mode = mode_client;
            nargc --;
        }
        else {
            usage();
            return EXIT_FAILURE;
        }
    }
    else {
        usage();
        return EXIT_FAILURE;
    }

    if(mode == mode_server)
    {
        while((c = getopt_long_only(nargc, argv + 1, "a:dp:h", longopts, NULL)) != -1)
        {
            switch (c) {
                case 'd':
                    delay = atoi(optarg);
                    break;
                case 'a':
                    client_addr = strdup(optarg);
                    break;
                case 'p':
                    port = strdup(optarg);
                    break;
                case 'h':
                    usage();
                    return EXIT_FAILURE;
            }
        }
    }

    else if(mode == mode_client)
    {
        while((c = getopt_long_only(nargc, argv + 1, "t:p:o:s:h", longopts, NULL)) != -1)
        {
            switch(c) {
                case 'i':
                    interface = strdup(optarg);
                    break;
                case 'p':
                    port = strdup(optarg);
                    break;
                case 'o':
                    file = strdup(optarg);
                    break;
                case 's':
                    sample = atoi(optarg);
                    break;
                case 'h':
                    usage();
                    return EXIT_FAILURE;
            }
        }
    }


    if(mode == mode_client)
    {
        // Validate input options
        if (!interface) {
            fprintf(stderr, "err: --interface is not specified. --help to print usage.\n");
            return EXIT_FAILURE;
        }

        if(!file) {
            fprintf(stderr, "err: --out_file is not specified. --help to print usage.\n");
            return EXIT_FAILURE;
        }
        
        if(!port) {
            fprintf(stderr, "err: --port is not specified. --help to print usage.\n");
            return EXIT_FAILURE;
        }

        socks = pktgen_client_init(interface, atoi(port), file, &fp);
        if(!socks) {
            fprintf(stderr, "pktgen client init failed.\n");
            return EXIT_FAILURE;
        }
        // Disable file buffer
        setvbuf(fp, NULL, _IONBF, 0);

        ret = pktgen_client_run(socks, interface, sample, fp);
    }

    else if(mode == mode_server)
    {
        // Validate input options
        if(!client_addr) {
            fprintf(stderr, "err: --addr is not specified. --help to print usage.\n");
            return EXIT_FAILURE;
        }

        if (!port) {
            fprintf(stderr, "err: --port is not specified. --help to print usage.\n");
            return EXIT_FAILURE;
        }

        if(client_addr)
        {
            socks = pktgen_server_init(atoi(port));
            if(!socks) {
                fprintf(stderr, "pktgen server init failed.\n");
                return EXIT_FAILURE;
        }
            ret = pktgen_server_run(socks, client_addr, port, delay);
        }
    }

    close(socks->udp_sock);    
    fcloseall();
    
    return ret;
}

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
#include <pthread.h>

#include "pktgen.h"

enum rtt_pktgen_mode {
    mode_none = 0,
    mode_server,
    mode_client
};

// multithread variables - client
char write_buf_global[49152] = {0};
int write_buf_global_pos = 0;

bool snapshot_th_exist = false;

pthread_mutex_t write_lock;
pthread_mutex_t snapshot_lock;

FILE *log_file;
FILE *snapshot_trigger = NULL;
int snapshot_cnt = 1;




static struct option longopts[] ={
    {"interface", required_argument, NULL, 'i'},
    {"delay", required_argument, NULL, 'd'},
    {"addr", required_argument, NULL, 'a'},
    {"port", required_argument, NULL, 'p'},
    {"out_file", required_argument, NULL, 'o'},
    {"sample", required_argument, NULL, 's'},
    {"runtime", required_argument, NULL, 'r'},
    {"warmup", required_argument, NULL, 'w'},
    {"threshold", required_argument, NULL, 't'},
    {"cpulist", required_argument, NULL, 'c'},
    {"cont_snapshot", no_argument, NULL, 'n'},
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
    fprintf(stderr, "--sample: CLIENT ONLY, number of sample data. -1 is inf, default: -1\n");
    fprintf(stderr, "--runtime: CLIENT ONLY, set application running time (sec). -1 is inf, default: -1\n");
    fprintf(stderr, "--warmup: CLIENT ONLY, set warm-up counter. a number of packets will be dropped. default:100 \n");
    fprintf(stderr, "--threshold: CLIENT ONLY, set threshold of total latency (us). If latency excceed this threshold value, take ftrace snapshot. default: -1(Disabled)\n");
    fprintf(stderr, "--cpulist: CLIENT ONLY, set cpu affinity to threads. max 256cpus, split by ',' delimiter.\ne.g) 2,3,4,5 (set cpu 2,3,4,5 to each threads)\n");
    fprintf(stderr, "--cont_snapshot: CLIENT ONLY, continuously take ftrace snapshot until runtime is end.\n\n");
}

void* write_log()
{
    struct timespec ts_sleep = {0,};
    int ret;
    while (1)
    {
        pthread_mutex_lock(&write_lock);
        if (write_buf_global_pos != 0)
        {
            fwrite(write_buf_global, sizeof(char),write_buf_global_pos, log_file);
            write_buf_global_pos = 0;
        }
        pthread_mutex_unlock(&write_lock);

        ts_sleep.tv_sec = 0;
        ts_sleep.tv_nsec = 2000000; // sleep 2ms
        do {
            ret = nanosleep(&ts_sleep, &ts_sleep);
        } while (ret && errno == EINTR);
    }
    return NULL;
}

void* write_snapshot()
{
    struct timespec ts_sleep = {0,};
    FILE *snapshot_data = NULL;
    char buf[4096];
    char fname[25] = "ftrace-snapshot-";
    int len = 0;
    int ret;

    // wait 1ms to flush buffer
    nanosleep(&ts_sleep, NULL);
    ts_sleep.tv_nsec = 1000000;

    if(!(snapshot_trigger = fopen("/sys/kernel/debug/tracing/snapshot", "w")))
    {
        fprintf(stderr, "err: cannot open ftrace snapshot file in snapshot thread.\n");
        goto finalize;
    }

    fwrite("1", 1, 1, snapshot_trigger);
    fclose(snapshot_trigger);
    fprintf(stderr, "ftrace snapshot %d has been taken!\n", snapshot_cnt);

    // snapshot trigger is valid?
    while(!(snapshot_trigger = fopen("/sys/kernel/debug/tracing/snapshot", "r")))
    {
        fprintf(stderr, "err: cannot open ftrace snapshot file.\n");
        ts_sleep.tv_sec = 0;
        ts_sleep.tv_nsec = 100000000; // sleep 100ms
        do {
            ret = nanosleep(&ts_sleep, &ts_sleep);
        } while (ret && errno == EINTR);
    }

    // copy snapshot to file
    len = strlen(fname);
    sprintf(fname + len, "%d", snapshot_cnt);
    snapshot_cnt++;

    snapshot_data = fopen(fname, "w");
    if(!snapshot_data) goto finalize;

    int nread;
    while (feof(snapshot_trigger) == 0)
    {
        nread = fread(buf, sizeof(char), sizeof(buf), snapshot_trigger);
        fwrite(buf, sizeof(char), nread, snapshot_data);
    }

    fclose(snapshot_data);
    fclose(snapshot_trigger);
    // clear snapshot
    snapshot_trigger = fopen("/sys/kernel/debug/tracing/snapshot", "w");
    fwrite("0", 1, 1, snapshot_trigger);
    fclose(snapshot_trigger);

    
finalize:
    pthread_mutex_lock(&snapshot_lock);
    snapshot_th_exist = false;
    pthread_mutex_unlock(&snapshot_lock);
    return NULL;
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

int pktgen_client_run(struct pktgen_socks *socks, char* interface, int64_t sample, FILE *fp, int run_time, int warmup, int snapshot_threshold, int *arr_cpu, int16_t cpu_cnt, bool cont_snapshot)
{
    // TODO: Remove RX Timestamp
    int ret;
    bool timer = false;
    bool ftrace_snapshot = false;
    float discard_cnt = 0;

    pthread_t write_thread;
    pthread_t snapshot_thread;
    cpu_set_t cpuset;

    struct pktgen_msg msg = {0,};
    struct sockaddr_in remaddr = {0,};
    struct timespec rx_timestamp = {0,};
    struct timespec curr_timestamp = {0,};
    struct timespec ts_sleep = {0,};
    uint64_t end_time = 0;
    uint64_t curr_time = 0;
    char write_buf_local[16384] = {0};
    int write_buf_pos = 0;

    FILE *set_ftrace_pid = NULL;

    int i;

    // Initialize mutex
    pthread_mutex_init(&write_lock, NULL);
    pthread_mutex_init(&snapshot_lock, NULL);

    // Initialize write Thread
    pthread_create(&write_thread, NULL, write_log, NULL);
    if (cpu_cnt != 0)
    {
        fprintf(stdout, "info: Write thread allocated to cpu %d.\n", arr_cpu[cpu_cnt-1]);
        CPU_ZERO(&cpuset);
        CPU_SET(arr_cpu[cpu_cnt-1], &cpuset);
        if(pthread_setaffinity_np(write_thread, sizeof(cpuset), &cpuset) != 0)
        {
            fprintf(stderr, "err: cpuset failed.\n");
            exit(EXIT_FAILURE);
        }

    }
//    fprintf(stdout, "pid:%ld\n\n", (long)getpid());
    
    if (run_time != -1)
    {
        clock_gettime(CLOCK_MONOTONIC, &curr_timestamp);
        curr_time = curr_timestamp.tv_sec * 1000000000UL + curr_timestamp.tv_nsec;
        end_time = curr_time + run_time * 1000000000UL;
        timer = true;
    }

    if (snapshot_threshold != -1)
    {
        snapshot_threshold = snapshot_threshold * 1000; // us to ns
        ts_sleep.tv_nsec = 1000000; // 1ms sleep timer

        if((snapshot_trigger = fopen("/sys/kernel/debug/tracing/snapshot", "w")) < 0)
        {
            fprintf(stderr, "error: cannot open ftrace snapshot file. ftrace is activated?\n");
            fcloseall();
            exit(EXIT_FAILURE);
        }
        fprintf(stdout, "info: clear ftrace snapshot buffer...\n");

        fwrite("0", 1, 1, snapshot_trigger);
        fclose(snapshot_trigger);

        fprintf(stdout, "info: set PID to set_ftrace_pid\n");
        if((set_ftrace_pid = fopen("/sys/kernel/debug/tracing/set_ftrace_pid", "w")) < 0)
        {
            fprintf(stderr, "error: cannot open set_ftrace_pid. ftrace is activated?\n");
            fcloseall();
            exit(EXIT_FAILURE);
        }
//        fprintf(set_ftrace_pid, "%ld", (long)getpid());
        fprintf(stdout, "info: set PID %ld to set_ftrace_pid.\n", (long)getpid());
        fclose(set_ftrace_pid);
        sleep(1);

        if (cont_snapshot) fprintf(stdout, "info: take ftrace snapshot continuously.\n");

        ftrace_snapshot = true;
    }

    fprintf(stdout, "\nWaiting for warm-up...\n");
    client_warmup(socks, &msg, &rx_timestamp, &remaddr, warmup);
    fprintf(stdout, "Warm-up finished!\n\n");


    while(sample)
    {
        if(sample > 0) sample--;
        if((ret = receive_pkt_msg(socks->udp_sock, &msg, &rx_timestamp, &remaddr, true)) < 0) {
            fprintf(stderr, "client.receive_pkt_msg: invalid packet.\n");
            return EXIT_FAILURE;
        }
        // Get current time
        clock_gettime(CLOCK_MONOTONIC, &curr_timestamp);
        curr_time = curr_timestamp.tv_sec * 1000000000UL + curr_timestamp.tv_nsec;

        if(timer) {
            if(curr_time > end_time) break;
        }

        // local buffer has string, flush it.
        if(write_buf_pos != 0)
        {
            if(pthread_mutex_trylock(&write_lock) != EBUSY)
            {
                strncpy(write_buf_global + write_buf_global_pos, write_buf_local, write_buf_pos + 1); // write NULL char
                write_buf_global_pos = write_buf_global_pos + write_buf_pos;
                pthread_mutex_unlock(&write_lock);
                write_buf_pos = 0;
            }
        }

        // Write to local buffer
        write_buf_pos += sprintf(write_buf_local + write_buf_pos, "xdp_to_tc: %"PRId64"\n", msg.tc_ingress - msg.xdp_recv);
        write_buf_pos += sprintf(write_buf_local + write_buf_pos, "tc_to_iptables: %"PRId64"\n", msg.iptables_out - msg.tc_ingress);
        write_buf_pos += sprintf(write_buf_local + write_buf_pos, "iptables_to_app: %"PRId64"\n\n", curr_time - msg.iptables_out);

        // ftrace snapshot logic
        if (ftrace_snapshot)
        {
            if ((curr_time - msg.xdp_recv) > snapshot_threshold)
            {
                fprintf(stdout, "curr_time: %"PRId64"ns\n", curr_time);
                fprintf(stdout, "xdp_to_tc: %"PRId64"ns\n", msg.tc_ingress - msg.xdp_recv);
                fprintf(stdout, "tc_to_iptables: %"PRId64"ns\n", msg.iptables_out - msg.tc_ingress);
                fprintf(stdout, "iptables_to_app: %"PRId64"ns\n", curr_time - msg.iptables_out);
                fprintf(stdout, "total_diff:  %"PRId64"ns\n", (curr_time - msg.xdp_recv));
                clock_gettime(CLOCK_MONOTONIC, &curr_timestamp);
                curr_time = curr_timestamp.tv_sec * 1000000000UL + curr_timestamp.tv_nsec;

                if(!cont_snapshot)
                {
                    // wait 1ms to flush buffer
                    nanosleep(&ts_sleep, NULL);
                    ts_sleep.tv_nsec = 1000000;

                    if(!(snapshot_trigger = fopen("/sys/kernel/debug/tracing/snapshot", "w")))
                    {
                        fprintf(stderr, "err: cannot open ftrace snapshot file in main thread.\n");
                        continue;
                    }

                    fwrite("1", 1, 1, snapshot_trigger);
                    fcloseall();
                    fprintf(stderr, "ftrace snapshot %d has been taken!\n", snapshot_cnt);
                    exit(EXIT_SUCCESS);
                }

                else {

                    if (snapshot_th_exist)
                    {
                        fprintf(stdout, "warn: ftrace snapshot buffer is busy. snapshot skipped.\n");
                        goto discard_buf;
                    }

                    /* Create snapshot thread (non-joinable) */
                    pthread_mutex_lock(&snapshot_lock);
                    if ((pthread_create(&snapshot_thread, NULL, write_snapshot, NULL)) != 0)
                    {
                        fprintf(stderr, "err: cannot create snapshot thread.\n");
                        pthread_mutex_unlock(&snapshot_lock);
                        goto discard_buf;
                    }
                    pthread_detach(snapshot_thread);
                    snapshot_th_exist = true;
                    pthread_mutex_unlock(&snapshot_lock);

                    /* discard N packets (N = (latency / interval) * 2) */
discard_buf:
                    discard_cnt = ((curr_time - msg.xdp_recv) / 1000) / 100;
                    for(i = 0; i < (int)discard_cnt * 2; i++)
                    {
                         if((ret = receive_pkt_msg(socks->udp_sock, &msg, &rx_timestamp, &remaddr, true)) < 0) {
                            fprintf(stderr, "client.receive_pkt_msg: invalid packet.\n");
                            return EXIT_FAILURE;
                         }
//                        fprintf(stdout, "debug: udp packet discarded.\n");
                    }
                }
            }
        }   
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct pktgen_socks *socks = NULL;
    enum rtt_pktgen_mode mode = mode_none;
    char *interface = NULL;
    int delay = 0;
    char *client_addr = NULL;
    char *port = NULL;
    char *file = NULL;
    int c, nargc = argc;
    int64_t sample = -1;
    int runtime = -1;
    int warmup_cnt = 100;
    int threshold = -1;

    
    char *cpuptr; // optarg pointer
    int16_t cpuptr_pos = 0;
    int cpu_list[256] = {0};

    bool cont_snapshot = false;

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
        while((c = getopt_long_only(nargc, argv + 1, "t:p:o:s:r:n:h", longopts, NULL)) != -1)
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
                case 'r':
                    runtime = atoi(optarg);
                    break;
                case 'w':
                    warmup_cnt = atoi(optarg);
                    break;
                case 't':
                    fprintf(stdout, "Ftrace snapshot trigger enabled!\nplease ensure target ftrace function is active.\n");
                    threshold = atoi(optarg);
                    break;
                case 'c':
                    cpuptr = strtok(optarg, ",");
                    while(cpuptr != NULL)
                    {
                        cpu_list[cpuptr_pos] = atoi(cpuptr);
                        cpuptr = strtok(NULL, ",");
                        cpuptr_pos++;
                        if(cpuptr_pos == 256)
                        {
                            fprintf(stderr, "warning: number of elements in cpulist over 256. after 256th cpu will be ignored.\n");
                            break;
                        }
                    }
                    break;
                case 'n':
                    cont_snapshot = true;
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

        socks = pktgen_client_init(interface, atoi(port), file, &log_file);
        if(!socks) {
            fprintf(stderr, "pktgen client init failed.\n");
            return EXIT_FAILURE;
        }
        // Disable file buffer
        setvbuf(log_file, NULL, _IONBF, 0);

        ret = pktgen_client_run(socks, interface, sample, log_file, runtime, warmup_cnt, threshold, cpu_list, cpuptr_pos, cont_snapshot);
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

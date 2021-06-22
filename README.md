# In-kernel packet latency teardown w/ eBPF
This repo contains source code for in-kernel packet latency teardown method that based on eBPF.
Each eBPF applications should be attached to appropriate hook point.

## How to compile it?
to compile eBPF bytecode, we need to use LLVM backend.
here's example of compile command.
### Prerequisite


    // LLVM 7.0
    yum install centos-release-scl
    yum-config-manager --enable rhel-server-rhscl-7-rpms
    yum install llvm-toolset-7.0
    scl enable llvm-toolset-7.0 bash
  ### Compile 
    clang -O2 -emit-llvm -c SOURCE_CODE -o - | llc -march=bpf -filetype=obj -o OBJECT_FILE

## How to Attach it?
Each eBPF applications MUST attach to right Hook Points.
We're using iproute2, bpftool and BPF Filesystem.
### Prerequisite
- iptables 1.6.2 w/libxt option
-  bpftool, libbpf

        
        // bpftool, libbpf
        cd LINUX_KERNEL_SOURCE_DIR
        cd tools/bpf/bpftool
        make
        make install
        
        cd LINUX_KERNEL_SOURCE_DIR
        cd tools/lib/bpf
        make
        make install
        
        // iptables 1.6.2
        wget https://www.netfilter.org/projects/iptables/files/iptables-1.6.2.tar.bz2 tar -xf iptables-1.6.2.tar.bz2
        cd iptables-1.6.2
        ./configure --prefix=/usr \ --sbindir=/sbin \ --disable-nftables \ --enable-libipq \ --with-xtlibdir=/lib/xtables &&  
        make
        # Install libxt_bpf.so
        cp extensions/libxt_bpf.so /usr/lib/x86_64-linux-gnu/xtables/libxt_bpf.so

### Attach
    # Attach XDP module
    ip link set dev <dev_name> xdp obj 1-xdp_timestamp_in_pkt.o
    
    # Attach TC filter
    tc qdisc add dev <dev_name> clsact
    tc filter add dev <dev_name> ingress prio 1 handle 1 bpf da obj 2-tc_ingress_timestamp_in_pkt.o
    
    # Attach iptables socket filter
    bpftool prog load 3-iptables_timestamp_in_pkt.o /sys/fs/bpf/iptables_timestamp_in_pkt type socket
    iptables -N TSTAMP
    iptables -A INPUT -i <dev_name> -p udp --dport 5000 -j TSTAMP
    iptables -A TSTAMP -i <dev_name> -m bpf --object-pinned /sys/fs/bpf/iptables_timestamp_in_pkt -j LOG

### Detach
    # Detach XDP module
    ip link set dev <dev_name> xdp off
    # Detach TC filter
    tc filter del dev <dev_name> ingress prio 1 handle 1 bpf
    tc qdisc del dev <dev_name> clsact
    # Detach iptables socket filter
    iptables -D TSTAMP -i <dev_name> -m bpf --object-pinned /sys/fs/bpf/iptables_timestamp_in_pkt -j LOG
    iptables -X TSTAMP
    rm /sys/fs/bpf/iptables_timestamp_in_pkt
#!/bin/bash
echo "function" > /sys/kernel/debug/tracing/current_tracer
echo "mono" > /sys/kernel/debug/tracing/trace_clock
echo 1 > /sys/kernel/debug/tracing/free_buffer
sleep 1
echo "1024" > /sys/kernel/debug/tracing/buffer_size_kb

sleep 1

echo "bpf_check" > /sys/kernel/debug/tracing/set_ftrace_filter
echo "bpf_prog_load" >>  /sys/kernel/debug/tracing/set_ftrace_filter
echo "bpf_prog_add" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "generic_xdp_install" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "dev_change_xdp_fd" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "dev_xdp_install" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "bpf_prog_release" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "bpf_prog_free" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "__dev_xdp_query" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "rtnl_xdp_report_one" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "rtnl_xdp_prog_drv" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "veth_xdp" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "bpf_prog_get_stats*" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "rtnl_xdp_prog_hw" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "rtnl_xdp_prog_skb" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "mlx5e_xdp" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "dev_xdp_install" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "mlx5e_xdp_set" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "*__do_sys_bpf*" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "1" > /sys/kernel/debug/tracing/tracing_on

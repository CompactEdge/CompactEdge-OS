#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include "bpf_helpers.h"

#include <stdint.h>
#define SEC(NAME) __attribute__((section(NAME), used))

#define bpf_htons(x) ((__be16)___constant_swab16((x)))
#define bpf_htonl(x) ((__be32)___constant_swab32((x)))
#define memcpy(dest, src, n) __builtin_memcpy((dest), (src), (n))

__attribute__((__always_inline__))
static inline void ipv4_l4_csum(void *data_start, __u32 data_size,
                                __u64 *csum, struct iphdr *iph) {
  __u32 tmp = 0;
  *csum = bpf_csum_diff(0, 0, &iph->saddr, sizeof(__be32), *csum);
  *csum = bpf_csum_diff(0, 0, &iph->daddr, sizeof(__be32), *csum);
  tmp = __builtin_bswap32((__u32)(iph->protocol));
  *csum = bpf_csum_diff(0, 0, &tmp, sizeof(__u32), *csum);
  tmp = __builtin_bswap32((__u32)(data_size));
  *csum = bpf_csum_diff(0, 0, &tmp, sizeof(__u32), *csum);
  *csum = bpf_csum_diff(0, 0, data_start, data_size, *csum);
  *csum = csum_fold_helper(*csum);
}

SEC("prog")
int xdp_print_timestamp(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth = data;
		
	uint64_t nh_off = sizeof(*eth);
	if (data + nh_off > data_end) {
		return XDP_PASS;
	}
	uint16_t h_proto = eth->h_proto;
	
	if(h_proto == bpf_htons(ETH_P_IP)) {
		struct iphdr *iph = data + nh_off;
		struct udphdr *udph = data + nh_off + sizeof(struct iphdr);
		if(udph + 1 > (struct udphdr *)data_end) {
			return XDP_PASS;
		}
		if(iph->protocol == IPPROTO_UDP && ((udph->dest) == bpf_htons(32767))) // set xdp_port here.
		{
			uint64_t time = bpf_ktime_get_ns(); // ingress time
			void *payload = (void*)(long)udph + sizeof(struct udphdr);
			if(payload + 8 > data_end) {
				return XDP_PASS;
			}
			*(uint64_t*)payload = time;
			udph->check = 0;
			return XDP_PASS;
		}
	}
	return XDP_PASS;
}

char _license[] SEC("license") = "GPL";

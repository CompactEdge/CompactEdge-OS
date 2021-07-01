FIRST_SENTENCE="xdp_to_tc"
SECOND_SENTENCE="tc_to_iptables"
THIRD_SENTENCE="iptables_to_cont"
FORTH_SENTENCE="cont_to_app"

import argparse
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('--file', required=True, help='input file name')

args = parser.parse_args()

f = open(args.file)

# initialize variables
max_xdp_tc_latency = 0
mean_xdp_tc_latency = 0
xdp_tc_latency_line = 0

max_tc_iptables_latency = 0
mean_tc_iptables_latency = 0
tc_iptables_line = 0

max_iptables_cont_latency = 0
mean_iptables_cont_latency = 0
iptables_cont_line = 0

max_cont_app_latency = 0
mean_cont_app_latency =0
cont_app_line = 0

max_total_latency = 0
total_latency_line = 0

lst_xdp_to_tc = []
lst_tc_to_iptables = []
lst_iptables_to_cont = []
lst_cont_to_app = []
lst_total_latency = []


sample_num = 0

for i, line in enumerate(f):

    if FIRST_SENTENCE in line:
        xdp_to_tc = int(line.split(': ')[1])
        lst_xdp_to_tc.append(xdp_to_tc)
        if xdp_to_tc > max_xdp_tc_latency:
            max_xdp_tc_latency = xdp_to_tc
            xdp_tc_latency_line = i
        continue

    if SECOND_SENTENCE in line:
        tc_to_iptables = int(line.split(': ')[1])
        lst_tc_to_iptables.append(tc_to_iptables)
        if tc_to_iptables > max_tc_iptables_latency:
            max_tc_iptables_latency = tc_to_iptables
            tc_iptables_line = i
        continue

    if THIRD_SENTENCE in line:
        iptables_to_cont = int(line.split(': ')[1])
        lst_iptables_to_cont.append(iptables_to_cont)
        if iptables_to_cont > max_iptables_cont_latency:
            max_iptables_cont_latency = iptables_to_cont
            iptables_cont_line = i
        continue

    if FORTH_SENTENCE in line:
        cont_to_app = int(line.split(': ')[1])
        lst_cont_to_app.append(cont_to_app)
        if cont_to_app > max_cont_app_latency:
            max_cont_app_latency = cont_to_app
            cont_app_line = i

        lst_total_latency.append((xdp_to_tc + tc_to_iptables + iptables_to_cont + cont_to_app))
        if (xdp_to_tc + tc_to_iptables + iptables_to_cont + cont_to_app) > max_total_latency:
            max_total_latency = (xdp_to_tc + tc_to_iptables + iptables_to_cont + cont_to_app)
            total_latency_line = i - 3
        sample_num += 1
        continue

print()
print("Total Sample: %d\n\n" % (sample_num))
print("MAX XDP-TC Ingress Latency: %dns" % (max_xdp_tc_latency))
print("MEAN XDP-TC Ingress Latency: %dns" % (np.average(lst_xdp_to_tc)))
print("MAX XDP-TC Ingress Latency Line: %d\n" % (xdp_tc_latency_line))

print("MAX TC Ingress-iptables Latency: %dns" % (max_tc_iptables_latency))
print("MEAN TC Ingress-iptables Latency: %dns" % (np.average(lst_tc_to_iptables)))
print("MAX TC Ingress-iptables Latency Line: %d\n" % (tc_iptables_line))

print("MAX iptables-Container Latency: %dns" % (max_iptables_cont_latency))
print("MEAN iptables-Container Latency: %dns" % (np.average(lst_iptables_to_cont)))
print("MAX iptables-Container Latency Line: %d\n" % (iptables_cont_line))

print("MAX Container-App Latency: %dns" % (max_cont_app_latency))
print("MEAN Container-App Latency: %dns" % (np.average(lst_cont_to_app)))
print("MAX Container-App Latency Line: %d\n" % (cont_app_line))

print("MAX Total Latency: %dns" % (max_total_latency))
print("MEAN Total Latency: %dns" % (np.average(lst_total_latency)))
print("MAX Total Latency Line: %d\n" % (total_latency_line))

print("================================Tail Latency Data================================")
print()
print("XDP-TC Ingress")
print("P95: %dns" % np.percentile(lst_xdp_to_tc, 95))
print("P99: %dns" % np.percentile(lst_xdp_to_tc, 99))
print("P99.9: %dns" % np.percentile(lst_xdp_to_tc, 99.9))
print("P99.99: %dns" % np.percentile(lst_xdp_to_tc, 99.99))
print("P99.999: %dns" % np.percentile(lst_xdp_to_tc, 99.999))
print()
print("TC Ingress-iptables")
print("P95: %dns" % np.percentile(lst_tc_to_iptables, 95))
print("P99: %dns" % np.percentile(lst_tc_to_iptables, 99))
print("P99.9: %dns" % np.percentile(lst_tc_to_iptables, 99.9))
print("P99.99: %dns" % np.percentile(lst_tc_to_iptables, 99.99))
print("P99.999: %dns" % np.percentile(lst_tc_to_iptables, 99.999))
print()
print("iptables-Cont")
print("P95: %dns" % np.percentile(lst_iptables_to_cont, 95))
print("P99: %dns" % np.percentile(lst_iptables_to_cont, 99))
print("P99.9: %dns" % np.percentile(lst_iptables_to_cont, 99.9))
print("P99.99: %dns" % np.percentile(lst_iptables_to_cont, 99.99))
print("P99.999: %dns" % np.percentile(lst_iptables_to_cont, 99.999))
print()
print("Cont-App")
print("P95: %dns" % np.percentile(lst_cont_to_app, 95))
print("P99: %dns" % np.percentile(lst_cont_to_app, 99))
print("P99.9: %dns" % np.percentile(lst_cont_to_app, 99.9))
print("P99.99: %dns" % np.percentile(lst_cont_to_app, 99.99))
print("P99.999: %dns" % np.percentile(lst_cont_to_app, 99.999))


print()
print("Done.\n")

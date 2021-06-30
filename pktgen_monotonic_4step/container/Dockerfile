ARG ARCH="amd64"
ARG OS="linux"
FROM quay.io/prometheus/busybox-${OS}-${ARCH}:glibc

COPY pktgen /bin/pktgen
COPY pktgen_run.sh /bin/pktgen_run.sh
USER	root
EXPOSE  5000/UDP
ENTRYPOINT ["/bin/pktgen_run.sh"]

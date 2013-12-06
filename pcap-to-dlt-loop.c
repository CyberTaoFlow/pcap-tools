#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pcap.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>

#include "pcap_layers.h"

const char *progname;
struct pcap_pkthdr out_hdr;
const u_char *out_data;

/* Copy the host order 32 bit integer in x into the memory pointed to by p
 * in network order.  p does not have to be aligned. */
#ifndef htonpl
#define htonpl(p, x) \
    do { \
        ((uint8_t*)(p))[0] = (x & 0xFF000000) >> 24; \
        ((uint8_t*)(p))[1] = (x & 0x00FF0000) >> 16; \
        ((uint8_t*)(p))[2] = (x & 0x0000FF00) >> 8; \
        ((uint8_t*)(p))[3] = (x & 0x000000FF) >> 0; \
    } while (0)
#endif


/*
 * this will only be called if 'ip' is a complete IPv header
 */
int
my_ip4_handler(const struct ip *ip4, int len, void *userdata)
{
    out_hdr.len = out_hdr.caplen = len + 4;
    out_data = (void*) ip4 - 4;
    htonpl(out_data, AF_INET);
    return 0;
}

int
my_ip6_handler(const struct ip6_hdr *ip6, int len, void *userdata)
{
    out_hdr.len = out_hdr.caplen = len + 4;
    out_data = (void*) ip6 - 4;
    htonpl(out_data, AF_INET6);
    return 0;
}

int
main(int argc, char *argv[])
{
    pcap_t *in = NULL;
    char errbuf[PCAP_ERRBUF_SIZE + 1];
    struct pcap_pkthdr hdr;
    const u_char *data;
    pcap_dumper_t *out = NULL;
    pcap_t *dead;

    progname = argv[0];
    if (argc < 2) {
	fprintf(stderr, "usage: %s pcapfiles\n", progname);
	exit(1);
    }

    in = pcap_open_offline(argv[1], errbuf);
    if (NULL == in) {
	fprintf(stderr, "%s: %s", argv[1], errbuf);
	exit(1);
    }
    dead = pcap_open_dead(DLT_LOOP, 65536);
    if (NULL == dead) {
	perror("pcap_open_dead");
	exit(1);
    }
    out = pcap_dump_open(dead, "-");
    if (NULL == out) {
	perror("stdout");
	exit(1);
    }
    pcap_layers_init(pcap_datalink(in), 0);
    callback_ipv4 = my_ip4_handler;
    callback_ipv6 = my_ip6_handler;
    while ((data = pcap_next(in, &hdr))) {
	    out_hdr.ts = hdr.ts;
	    out_hdr.len = 0;
	    out_hdr.caplen = 0;
	    out_data = NULL;
	    handle_pcap(NULL, &hdr, data);
	    if (out_data && out_hdr.len)
		pcap_dump((void *)out, &out_hdr, out_data);
    }
    pcap_close(in);
    pcap_dump_close(out);
    exit(0);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <arpa/inet.h>

/*
** Macros
*/

#define TRACE(...) \
    printf(__VA_ARGS__);

#define TRACEF(fmt, ...) \
    printf("%s: " fmt, __func__, ##__VA_ARGS__);

#define ERROR(fmt, ...) \
    printf("ERROR: " fmt, ##__VA_ARGS__);

#define ABORTIF(cond, ...) \
    if (cond) { \
        ERROR(__VA_ARGS__); \
        abort(); \
    }

/*
** Types
*/

struct options {
    const char *ifname;
    int promisc;
    int count;
};

/*
** Functions
*/

static void hexdump(unsigned char *buf, unsigned int len)
{
    unsigned int i;
    char dump[96] = "";
    char *dp = dump;

    for (i = 0; i < len; ++i) {
        if (sizeof(dump) > (dp - dump))
            dp += snprintf(dp, sizeof(dump) - (dp - dump),
                           "%s%02x", (i % 4 == 0 ? " " : ""), buf[i]);

        if ((i + 1) % 16 == 0 || i == (len - 1)) {
            TRACE("%04x:%s\n", 16 * (i / 16), dump);
            dump[0] = '\0';
            dp = dump;
        }
    }
}

static void set_promisc(int fd, const char *ifname)
{
    struct ifreq ifreq;
    int rc = 0;
    assert(fd > 0);
    assert(ifname);
    memset(&ifreq, 0, sizeof(ifreq));

    /* retrieve interface flags
     */
    strncpy(ifreq.ifr_name, ifname, sizeof(ifreq.ifr_name) - 1);
    rc = ioctl(fd, SIOCGIFFLAGS, &ifreq);
    ABORTIF(rc < 0, "Call to ioctl SIOCGIFFLAGS on ifname %s failed: %s\n",
            ifname, strerror(errno));

    /* set the interface up and promisc mode
     */
    ifreq.ifr_flags |= IFF_UP;
    ifreq.ifr_flags |= IFF_PROMISC;

    /* update interface flags
     */
    rc = ioctl(fd, SIOCSIFFLAGS, &ifreq);
    ABORTIF(rc < 0, "Call to ioctl SIOCSIFFLAGS on ifname %s failed: %s\n",
            ifname, strerror(errno));
    TRACE("promiscuous mode set on %s\n", ifname);
}

static int get_index(int fd, const char *ifname)
{
    struct ifreq ifreq;
    int rc = 0;
    assert(fd > 0);
    assert(ifname);
    memset(&ifreq, 0, sizeof(ifreq));

    /* retrieve interface index by name
     */
    strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);
    rc = ioctl(fd, SIOCGIFINDEX, &ifreq);
    ABORTIF(rc < 0, "Call to ioctl SIOCGIFINDEX on ifname %s failed: %s\n",
            ifname, strerror(errno));

    TRACE("ifname = %s, ifindex = %d\n", ifname, ifreq.ifr_ifindex);
    return ifreq.ifr_ifindex;
}

static void ifdump(const struct options* opt)
{
    int fd, rc, count;
    int ifindex = -1;
    struct sockaddr_ll sa;

    /* create socket
     */
    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    ABORTIF(fd < 0, "socket() failed: %s\n", strerror(errno));
    TRACE("socket() succeeded, fd = %d\n", fd);

    /* set promisc mode if needed
     */
    if (opt->promisc) {
        set_promisc(fd, opt->ifname);
    }

    /* get interface index by name
     */
    ifindex = get_index(fd, opt->ifname);

    /* bind socket to the interface
     */
    memset(&sa, 0, sizeof(sa));
    sa.sll_ifindex = ifindex;
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);

    rc = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
    ABORTIF(rc !=0 , "bind() failed: %s\n", strerror(errno));
    TRACE("bind() succeeded, socket fd = %d bound to ifindex = %d\n",
          fd, ifindex);

    unsigned char buf[4096];
    ssize_t len;
    for (count = 0; !opt->count || count < opt->count; count++) {
        if ((len = recv(fd, buf, sizeof(buf), 0)) > 0) {
            TRACE("\nPacket %04d received, %ld bytes\n", count + 1, len);
            hexdump(buf, len);
        } else {
            TRACE("tick\n");
        }
    }

    close(fd);
}

void usage(const char* name)
{
    fprintf(stderr, "Usage: %s [-p] [-c count] ifname\n", name);
    exit(EXIT_FAILURE);
}

/*
** Main
*/

int main(int argc, char *argv[])
{
    int optname;
    struct options opt;
    memset(&opt, 0, sizeof(opt));

    while ((optname = getopt(argc, argv, "pc:")) != -1) {
        switch (optname) {
            case 'p':
                opt.promisc = 1;
                break;
            case 'c':
                opt.count = atoi(optarg);
                if (opt.count <= 0) {
                    usage(argv[0]);
                }
                break;
            default:
                usage(argv[0]);
                break;
        }
    }
    if (optind >= argc) {
        usage(argv[0]);
    }
    opt.ifname = argv[optind];

    ifdump(&opt);
    return 0;
}

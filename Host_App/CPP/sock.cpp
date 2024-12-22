#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <iostream>

#define NETLINK_USER 31
#define MAX_PAYLOAD 1024

int main() {
    int sock_fd;
    struct sockaddr_nl src_addr, dst_addr;
    struct nlmsghdr *nlh;
    struct iovec iov;
    struct msghdr msg;

    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0) {
        std::cout << "Could not create socket" << std::endl;
        return -1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    if (bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        std::cout << "Could not bind the socket" << std::endl;
        close(sock_fd);
        return -2;
    }

    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.nl_family = AF_NETLINK;
    dst_addr.nl_pid = 0;
    dst_addr.nl_groups = 0;
    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (!nlh) {
        std::cout << "Could not allocate memory for NL header" << std::endl;
        close(sock_fd);
        return -3;
    }

    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    strcpy((char*)NLMSG_DATA(nlh), "Hello from CPP");

    iov.iov_base = (void*)nlh;
    iov.iov_len = nlh->nlmsg_len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&dst_addr;
    msg.msg_namelen = sizeof(dst_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (sendmsg(sock_fd, &msg, 0) < 0) {
        std::cout << "Could not send message" << std::endl;
        free(nlh);
        close(sock_fd);
        return -4;
    }

    // Receive message
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    if (recvmsg(sock_fd, &msg, 0) < 0) {
        std::cout << "Could not receive message" << std::endl;
        free(nlh);
        close(sock_fd);
        return -5;
    }
    std::cout << "Received message: " << (char*)NLMSG_DATA(nlh) << std::endl;

    free(nlh);
    close(sock_fd);
    return 0;
}

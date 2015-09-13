/////////////////////////////////////////////////////////////////////////////////////////
//
// This is a quick and dirty implementation of an echo client/server.
// It isn't written to reflect best practices!
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZED_NET_IMPLEMENTATION
#include "zed_net.h"

int run_server(unsigned short port) {
    zed_net_init();

    zed_net_udp_socket_t *socket = zed_net_udp_socket_open(port, 0);
    printf("Running echo server on port %d!\n", port);

    char buffer[256];

    while (strcmp(buffer, "done") != 0) {
        zed_net_address_t sender;

        int bytes_read = zed_net_udp_socket_receive(socket, &sender, &buffer, sizeof(buffer));
        if (bytes_read) {
            printf("Received %d bytes from %s:%d: %s\n", bytes_read, zed_net_host_to_str(sender.host), sender.port, buffer);

            printf("Echoing...\n");
            zed_net_udp_socket_send(socket, sender, buffer, sizeof(buffer));
        }
    }

    printf("Done!\n");
    zed_net_udp_socket_close(socket);
    
    zed_net_shutdown();

    return 0;
}

int run_client(const char *host, unsigned short port) {
    zed_net_init();

    zed_net_udp_socket_t *socket = zed_net_udp_socket_open(0, 0);


    zed_net_address_t address;
    if (zed_net_get_address(&address, host, port) != 0) {
        printf("Error: %s\n", zed_net_get_error());

        zed_net_udp_socket_close(socket);
        zed_net_shutdown();

        return -1;
    }

    printf("Running client! Type \"done\" to exit.\n");

    char buffer[256];

    while (strcmp(buffer, "done") != 0) {
        printf("Message: ");

        fgets(buffer, 256, stdin);
        buffer[strlen(buffer) - 1] = '\0';

        printf("Sending...\n");
        zed_net_udp_socket_send(socket, address, buffer, sizeof(buffer));

        zed_net_address_t sender;

        int bytes_read = zed_net_udp_socket_receive(socket, &sender, &buffer, sizeof(buffer));
        if (bytes_read) {
            printf("Received %d bytes from %s:%d: %s\n", bytes_read, zed_net_host_to_str(sender.host), sender.port, buffer);
        }
    }
    
    printf("Done!\n");
    zed_net_udp_socket_close(socket);
    
    zed_net_shutdown();

    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "-server") == 0) {
        return run_server((unsigned short) atoi(argv[2]));
    } else if (argc == 4 && strcmp(argv[1], "-client") == 0) {
        return run_client(argv[2], (unsigned short) atoi(argv[3]));
    }

    printf("Usage: echoz [-server [port] or -client [host] [port]\n");

    return 0;
}


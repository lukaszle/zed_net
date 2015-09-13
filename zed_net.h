/////////////////////////////////////////////////////////////////////////////////////////
//
// zed_net - v0.18 - public domain networking library
// (inspired by the excellent stb libraries: https://github.com/nothings/stb)
//
// This library is intended primarily for use in games and provides a simple wrapper
// around BSD sockets (Winsock 2.2 on Windows). Sockets can be set to be blocking or
// non-blocking.
//
// Only UDP sockets are supported at this time, but this may later expand to include TCP.
//
// VERSION HISTORY
//
//    0.18 (9/13/2015) minor polishing
//    0.17 (8/8/2015) initial release
//
// LICENSE
//
//    This software is in the public domain. Where that dedication is not recognized, you
//    are granted a perpetual, irrevocable license to copy, distribute, and modify this
//    file as you see fit.
//
// USAGE
//
//    #define the symbol ZED_NET_IMPLEMENTATION in *one* C/C++ file before the #include
//    of this file; the implementation will be generated in that file.
//
//    If you define the symbol ZED_NET_STATIC, then the implementation will be private to
//    that file.
//
//    Immediately after this block comment is the "header file" section. This section
//    includes documentation for each API function.
//

#ifndef INCLUDE_ZED_NET_H
#define INCLUDE_ZED_NET_H

#ifdef ZED_NET_STATIC
#define ZED_NET_DEF static
#else
#define ZED_NET_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////////////////
//
// INITIALIZATION AND SHUTDOWN
//

// Get a brief reason for failure
ZED_NET_DEF const char *zed_net_get_error(void);

// Perform platform-specific socket initialization;
// *must* be called before using any other function
//
// Returns 0 on success, -1 otherwise (call 'zed_net_get_error' for more info)
ZED_NET_DEF int zed_net_init(void);

// Perform platform-specific socket de-initialization;
// *must* be called when finished using the other functions
ZED_NET_DEF void zed_net_shutdown(void);

/////////////////////////////////////////////////////////////////////////////////////////
//
// INTERNET ADDRESS API
//

// Represents an internet address usable by sockets
typedef struct {
    unsigned int host;
    unsigned short port;
} zed_net_address_t;

// Obtain an address from a host name and a port
//
// 'host' may contain a decimal formatted IP (such as "127.0.0.1"), a human readable
// name (such as "localhost"), or NULL for the default address
//
// Returns 0 on success, -1 otherwise (call 'zed_net_get_error' for more info)
ZED_NET_DEF int zed_net_get_address(zed_net_address_t *address, const char *host, unsigned short port);

// Converts an address's host name into a decimal formatted string
//
// Returns NULL on failure (call 'zed_net_get_error' for more info)
ZED_NET_DEF const char *zed_net_host_to_str(unsigned int host);

/////////////////////////////////////////////////////////////////////////////////////////
//
// UDP SOCKETS API
//

// Wraps the system handle for a UDP socket
typedef struct {
    int handle;
} zed_net_udp_socket_t;

// Opens a UDP socket and binds it to a specified port
// (use 0 to select a random open port)
//
// Socket will not block if 'non-blocking' is non-zero
//
// Returns NULL on failure (call 'zed_net_get_error' for more info)
ZED_NET_DEF zed_net_udp_socket_t *zed_net_udp_socket_open(unsigned int port, int non_blocking);

// Closes a previously opened socket
ZED_NET_DEF void zed_net_udp_socket_close(zed_net_udp_socket_t *socket);

// Sends a specific amount of data to 'destination'
//
// Returns 0 on success, -1 otherwise (call 'zed_net_get_error' for more info)
ZED_NET_DEF int zed_net_udp_socket_send(zed_net_udp_socket_t *socket, zed_net_address_t destination, const void *data, int size);

// Receives a specific amount of data from 'sender'
//
// Returns the number of bytes received, -1 otherwise (call 'zed_net_get_error' for more info)
ZED_NET_DEF int zed_net_udp_socket_receive(zed_net_udp_socket_t *socket, zed_net_address_t *sender, void *data, int size);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_ZED_NET_H

#ifdef ZED_NET_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "wsock32.lib")
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

static const char *zed_net__g_error;

static int zed_net__error(const char *message) {
    zed_net__g_error = message;

    return -1;
}

ZED_NET_DEF const char *zed_net_get_error(void) {
    return zed_net__g_error;
}

ZED_NET_DEF int zed_net_init(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        return zed_net__error("Windows Sockets failed to start");
    }

    return 0;
#else
    return 0;
#endif
}

ZED_NET_DEF void zed_net_shutdown(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

ZED_NET_DEF int zed_net_get_address(zed_net_address_t *address, const char *host, unsigned short port) {
    if (host == NULL) {
        address->host = INADDR_ANY;
    } else {
        address->host = inet_addr(host);
        if (address->host == INADDR_NONE) {
            struct hostent *hostent = gethostbyname(host);
            if (hostent) {
                memcpy(&address->host, hostent->h_addr, hostent->h_length);
            } else {
                return zed_net__error("Invalid host name");
            }
        }
    }

    address->port = port;
    
    return 0;
}

ZED_NET_DEF const char *zed_net_host_to_str(unsigned int host) {
    struct in_addr in;
    in.s_addr = host;

    return inet_ntoa(in);
}

ZED_NET_DEF zed_net_udp_socket_t *zed_net_udp_socket_open(unsigned int port, int non_blocking) {
    zed_net_udp_socket_t *sock = malloc(sizeof(zed_net_udp_socket_t));
    if (!sock) {
        zed_net__error("Out of memory");
        return NULL;
    }

    // Create the socket
    sock->handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock->handle <= 0) {
        zed_net_udp_socket_close(sock);
        zed_net__error("Failed to create socket");
        return NULL;
    }

    // Bind the socket to the port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(sock->handle, (const struct sockaddr *) &address, sizeof(struct sockaddr_in)) != 0) {
        zed_net_udp_socket_close(sock);
        zed_net__error("Failed to bind socket");
        return NULL;
    }

    // Set the socket to non-blocking if neccessary
    if (non_blocking) {
#ifdef _WIN32
        if (ioctlsocket(sock->handle, FIONBIO, &non_blocking) != 0) {
            zed_net_udp_socket_close(sock);
            zed_net__error("Failed to set socket to non-blocking");
            return NULL;
        }
#else
        if (fcntl(sock->handle, F_SETFL, O_NONBLOCK, non_blocking) != 0) {
            zed_net_udp_socket_close(sock);
            zed_net__error("Failed to set socket to non-blocking");
            return NULL;
        }
#endif
    }

    return sock;
}

ZED_NET_DEF void zed_net_udp_socket_close(zed_net_udp_socket_t *socket) {
    if (!socket) {
        return;
    }

    if (socket->handle) {
#ifdef _WIN32
        closesocket(socket->handle);
#else
        close(socket->handle);
#endif
    }

    free(socket);
}

ZED_NET_DEF int zed_net_udp_socket_send(zed_net_udp_socket_t *socket, zed_net_address_t destination, const void *data, int size) {
    if (!socket) {
        return zed_net__error("Socket is NULL");
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = destination.host;
    address.sin_port = htons(destination.port);

    int sent_bytes = sendto(socket->handle, (const char *) data, size, 0, (const struct sockaddr *) &address, sizeof(struct sockaddr_in));
    if (sent_bytes != size) {
        return zed_net__error("Failed to send data");
    }

    return 0;
}

ZED_NET_DEF int zed_net_udp_socket_receive(zed_net_udp_socket_t *socket, zed_net_address_t *sender, void *data, int size) {
    if (!socket) {
        return zed_net__error("Socket is NULL");
    }

#ifdef _WIN32
    typedef int socklen_t;
#endif

    struct sockaddr_in from;
    socklen_t from_length = sizeof(from);

    int received_bytes = recvfrom(socket->handle, (char *) data, size, 0, (struct sockaddr *) &from, &from_length);
    if (received_bytes <= 0) {
        return 0;
    }

    sender->host = from.sin_addr.s_addr;
    sender->port = ntohs(from.sin_port);

    return received_bytes;
}

#endif // ZED_NET_IMPLEMENTATION

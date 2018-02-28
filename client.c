#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <pthread.h>

#include "net/net_util.h"
#include "net/net_compat.h"

#include "ring.h"
#include "client.h"


/*
 * Open a non-blocking connected tcp socket and return it's fd.
 *
 * @return net_socket_fd_t, the socket file descriptor.
 *      Returns the macro NET_INVALID_SOCKET on failure.  No outstanding
 *      memory or sockets on failure.
 */
net_socket_fd_t
_client_tcp_connect(char *server_ip, char *server_port)
{
        net_socket_fd_t sock_fd;
        int err;
        struct addrinfo *server;
        struct addrinfo hints;

        /* The hints struct is used to specify what kind of server info we are
         * looking for--TCP/IP for this server example. */
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        /* getaddrinfo() gives us back a server address we can connect to.
           The first parameter is NULL since we want an address on this host.
           It actually gives us a linked list of addresses, but we'll just
           use the first. */
        if ( getaddrinfo(server_ip, server_port, &hints, &server) ) {
                net_error("Failed to get addrinfo.\n");
                return NET_INVALID_SOCKET;
        }

        sock_fd = net_socket_nonblocking(server->ai_family, server->ai_socktype,
                                         server->ai_protocol);

        if ( !NET_SOCKET_OK(sock_fd) ) {
                /* We use the following macro to read errno. */
                err = net_socket_errno(sock_fd);

                if (NET_SOCKET_ERRNO_IS_RESOURCE_LIMIT(err)) {
                        /* TODO: For OOS, set exhaustion flag to 1. */
                } else {
                        net_error("Socket creation failed: %s.\n",
                                  net_socket_strerror(err));
                }
                freeaddrinfo(server);
                return NET_INVALID_SOCKET;
        }

#ifndef _WIN32
        /* On Unix make the socket reusable.
         * This helps remove the "socket in already in use error."
         * 
         * TODO: The following is not necessarily proper in BSD. */
        if (net_socket_make_reuseable_unix(sock_fd) < 0) {
                /* For consistency use net_socket_errno() rather than errno,
                 * as on unix it is not necessary. */
                err = net_socket_errno(sock_fd);
                net_warn("Error setting SO_REUSEADDR flag: %s.\n",
                       net_socket_strerror(err));
                /* Let's continue; no fatal error. */
        }
#endif

        err = connect(sock_fd, server->ai_addr, server->ai_addrlen);
        freeaddrinfo(server);
        if(err < 0) {
                err = net_socket_errno(sock_fd);
                /* Is this a real error or just an non-blocking error? */
                if (!NET_SOCKET_ERRNO_IS_CONN_EINPROGRESS(err)) {
                        /* Yuck. Kill it. */
                        net_error("connect() to socket failed: %s.\n",
                                  net_socket_strerror(err));
                        net_socket_close(sock_fd);
                        return NET_INVALID_SOCKET;
                }
                /* Then it is an EINPROGRESS.
                 * The socket is nonblocking and the connection cannot
                 * be completed immediately.  It is possible to
                 * select(2) or poll(2) for completion by selecting the
                 * socket for writing.  After select(2) indicates
                 * writability, use getsockopt(2) to read the
                 * SO_ERROR option at level SOL_SOCKET to determine
                 * whether connect() completed successfully (SO_ERROR
                 * is zero) or unsuccessfully (SO_ERROR is one of the
                 * usual error codes listed here, explaining the reason
                 * for the failure). */
        }
        return sock_fd;
}


int
_client_tcp_loop(struct _server_vars *s_vars, int sock_fd)
{


        return 0;
}


int
client_start(char *server_ip, char *server_port)
{
        int sock_fd;
        int err;


        close(sock_fd);
        return err;
}

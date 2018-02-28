#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h> // memset
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <pthread.h>

#include "net/net_util.h"
#include "net/net_compat.h"

#include "ring.h"
#include "server.h"


/*
 * Open a tcp socket and return it's fd.  Don't forget to close it when done!
 * @return net_socket_fd_t, the socket of a binded and listening socket.
 *      Returns the macro NET_INVALID_SOCKET on failure.  No outstanding
 *      memory or sockets on failure.
 */
net_socket_fd_t
_server_tcp_init(char *server_port)
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
        hints.ai_flags = AI_PASSIVE;

        /* getaddrinfo() gives us back a server address we can connect to.
           The first parameter is NULL since we want an address on this host.
           It actually gives us a linked list of addresses, but we'll just
           use the first. */
        if (getaddrinfo(NULL, server_port, &hints, &server)) {
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

#ifdef _WIN32
        /* On Windows make the socket exclusive so that other Applications
         * cannot steal our port.  REUSEADDR in Windows is *not* safe. */
        if (net_socket_make_exclusive_win32(sock_fd) < 0) {
                // Tor devs forgot to use their net_socket_errno() abstraction
                err = net_socket_errno(sock_fd);
                net_error("Error setting SO_EXCLUSIVEADDRUSE flag: %s.\n",
                       net_socket_strerror(err));

                /* Tor is non-fatal here, but do not use the socket if it
                 * is not exclusive. */
                net_socket_close(sock_fd);
                freeaddrinfo(server);
                return NET_INVALID_SOCKET;
        }
#else
        /* Otherwise we're on a Unix based system.
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

        err = bind(sock_fd, server->ai_addr, server->ai_addrlen);
        freeaddrinfo(server);
        if (err < 0) {
                const char *helpfulhint = "";
                err = net_socket_errno(sock_fd);

                if (NET_SOCKET_ERRNO_IS_EADDRINUSE(err))
                        helpfulhint = " Is server already running?";

                net_error("Error binding to port: %s. %s.%s\n", server_port,
                          net_socket_strerror(err), helpfulhint);

                net_socket_close(sock_fd);
                return NET_INVALID_SOCKET;
        }

        /* SOMAXCONN is provivded by <sys/socket.h>. */
        if (listen(sock_fd, SOMAXCONN) < 0) {
                err = net_socket_errno(sock_fd);
                net_error("Error listening on port: %s. %s.\n", server_port,
                          net_socket_strerror(err));

                net_socket_close(sock_fd);
                return NET_INVALID_SOCKET;
        }

        return sock_fd;
}


void *
_server_tcp_nonblocking_worker(void *vars)
{
        /* Return value is an int that is stored as a (void *). */
        void *ret_val = 0;
        int client_fd;
        struct _server_vars *s_vars = (struct _server_vars *) vars;
        struct ring *rb = s_vars->rb;
        pthread_mutex_t *mutex_sum = &(s_vars->rb_mutex_sum);
        pthread_cond_t *not_full = &(s_vars->rb_not_full);
        pthread_cond_t *not_empty = &(s_vars->rb_not_empty);

        for(;;) {
                pthread_mutex_lock(mutex_sum);
                client_fd = rb_dequeue(rb);
                while (client_fd == -1) {
                        /* Try again! */
                        pthread_cond_wait(not_empty, mutex_sum);
                        client_fd = rb_dequeue(rb);
                }
                pthread_cond_signal(not_full);
                pthread_mutex_unlock(mutex_sum);

                // TODO: Do something with the connection.

server_nonblocking_worker_err:
                //SSL_free(ssl);
                // TODO: Fix EINTR
                close(client_fd);
        }

        return ret_val;
}


int
_server_tcp_loop(struct _server_vars *s_vars, int sock_fd, int thread_pool_size)
{
        int i;

        pthread_mutex_t *mutex_sum = &(s_vars->rb_mutex_sum);
        pthread_cond_t *not_full = &(s_vars->rb_not_full);
        pthread_cond_t *not_empty = &(s_vars->rb_not_empty);

        /**
        * TODO: Use tor's resizable array (container.h) for threads.
        */
        pthread_t threads[thread_pool_size];

        s_vars->rb = rb_create();
        if (!s_vars->rb) {
                fprintf(stderr, "Error creating ring buffer.\n");
                return -1;
        }
        pthread_mutex_init(mutex_sum, NULL);

        /* Create a thread pool */
        for(i = 0; i < thread_pool_size; i++) {
                int err = pthread_create(&threads[i], NULL,
                                         &_server_tcp_nonblocking_worker,
                                         s_vars);
                if (err) {
                        rb_delete(s_vars->rb);
                        pthread_mutex_destroy(mutex_sum);
                        fprintf(stderr, "Error creating pthread.\n");
                        return -1;
                }
        }

        /* Infinite loop for gathering requests */
        for(;;) {
                /* net_socket_fd_t, a macro to an int that is used to hold
                 * socket fds. */
                net_socket_fd_t client_fd;

                struct sockaddr client_addr;
                socklen_t addr_size = (socklen_t) sizeof(client_addr);

                /* For printing the connected client's ip address. */
                char ip[INET_ADDRSTRLEN];

                // TODO: Place accept handling into a seperate function.
                /* If the original sock_fd is blocking, then this accept will
                 * block.  Creates a new *nonblocking* socket. */
                client_fd = net_accept_nonblocking(sock_fd, &client_addr,
                                                   &addr_size);
                /* Check for error during accept() */
                if (!NET_SOCKET_OK(client_fd)) {
                        int err = net_socket_errno(conn->s);
                        if (NET_SOCKET_ERRNO_IS_ACCEPT_EAGAIN(err)) {
                                /* They hung up before we could accept();
                                 * that's fine.
                                 * TODO: Give the OOS (out-of-socket) handler
                                 * a chance to run. */
                                //connection_check_oos(n_open_sockets(), 0);
                                continue;
                        } else if (NET_SOCKET_ERRNO_IS_RESOURCE_LIMIT(err)) {
                                /* TODO: Exhaustion; tell the OOS handler. */
                                //connection_check_oos(n_open_sockets(), 1);
                                continue;
                        }
                        /* Otherwise there was a real error. */
                        net_error("accept() failed: %s. Closing server loop.\n",
                                 net_socket_strerror(err));
                        /* TODO: Mark ourselves for close. Have the central
                         * loop check for closed sockets to free.
                         * Use mutexes ... */

                        /* TODO: Tell the OOS handler about this too */
                        //connection_check_oos(n_open_sockets(), 0);
                        break;
                }
#ifdef _WIN32
                /* Do not set exclusive on accepted file descriptor.
                 * TODO: Why does Tor not need to make this exclusive?
                 * 
                 * Each exclusive socket must be shutdown.  Failure to do so
                 * can cause a denial of service attack. 
                 *
                 * A socket with SO_EXCLUSIVEADDRUSE set cannot always be
                 * reused immediately after socket closure. For example, if
                 * a listening socket with the exclusive flag set accepts a
                 * connection after which the listening socket is closed,
                 * another socket cannot bind to the same port as the first
                 * listening socket with the exclusive flag until the accepted
                 * connection is no longer active. */
#else
                if (net_socket_make_reuseable_unix(client_fd) < 0) {
                        if (net_socket_errno(client_fd) == EINVAL) {
                                /* This can happen on OSX if we get a badly
                                 * timed shutdown. */
                                net_debug("net_socket_make_reuseable_unix \
returned EINVAL.\n");
                        } else {
                                net_warn("Error setting SO_REUSEADDR flag. \
%s.\n", net_socket_strerror(errno));
                        }

                        net_socket_close(client_fd);
                        // Non-fatal error
                        continue;
                }
#endif 
                /* TODO: We accepted a new conn; run OOS handler. */
                //connection_check_oos(get_n_open_sockets(), 0);

                /* Print the client's ip */
                inet_ntop(AF_INET,
                          &((struct sockaddr_in *) &client_addr)->sin_addr,
                          ip, INET_ADDRSTRLEN);
                net_print("Accepted connection from %s.\n", ip);

                pthread_mutex_lock(mutex_sum);

                while(rb_enqueue(s_vars->rb, client_fd) != -1)
                        pthread_cond_wait(not_full, mutex_sum);

                pthread_cond_signal(not_empty);
                pthread_mutex_unlock(mutex_sum);
        }
        // TODO: Add an exit case and free threads

        rb_delete(s_vars->rb);
        pthread_mutex_destroy(mutex_sum);

        return 0;
}


int
server_start(char *server_port)
{
        int sock_fd;
        /* The initial size of the thread pool (TODO: Resizeable pool) */
        int thread_pool_size = 64;
        int err;

        struct _server_vars s_vars;
        // IMPORTANT: Populate s_vars
        // Create local scoped variables for addrinfo etc...

        /* Create a listening tcp/ip socket. */
        sock_fd = _server_tcp_init(server_port);
        if ( !NET_SOCKET_OK(sock_fd) ) {
                freeaddrinfo(server);
                return -1;
        }

        /* Zero out our variables */
        memset(&s_vars, 0, sizeof(struct _server_vars));

        /* Run server using the */
        err = _server_tcp_loop(&s_vars, sock_fd, thread_pool_size);

        close(sock_fd);
        return err;
}

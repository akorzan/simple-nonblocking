/** Modified and simplified tor_compat.h */

#ifndef _NET_COMPAT_H
#define _NET_COMPAT_H
#endif

/* --- Windows Macros for Networking Compatibility --- */
#ifdef _WIN32
/** Type used for a network socket.
 *
 * Actually, this should arguably be SOCKET; we use intptr_t here so that
 * any inadvertent checks for the socket being <= 0 or > 0 will probably
 * still work. */
#define net_socket_fd_t intptr_t
#define NET_SOCKET_FD_T_FORMAT INTPTR_T_FORMAT
/* --- Unix Macros for Networking Compatibility --- */
#else
/* Use "net_socket_fd_t fd;" instead of "int fd;" */
#define net_socket_fd_t int
#define NET_SOCKET_FD_T_FORMAT "%d"
#endif

int net_close(net_socket_fd_t sock_fd);
net_socket_fd_t net_socket_blocking(int domain, int type, int protocol);
net_socket_fd_t net_socket_nonblocking(int domain, int type, int protocol);
net_socket_fd_t net_accept_blocking(net_socket_fd_t sock_fd,
                                    struct sockaddr *addr, socklen_t *len);
net_socket_fd_t net_accept_nonblocking(net_socket_fd_t sock_fd,
                                       struct sockaddr *addr, socklen_t *len);

/* --- Windows Sockets ---
 * For historical reasons, windows sockets have an independent
 * set of errnos, and an independent way to get them.  Also, you can't
 * always believe WSAEWOULDBLOCK.  Use the macros below to compare
 * errnos against expected values, and use tor_socket_errno to find
 * the actual errno after a socket operation fails.
 */
#if defined(_WIN32)
/** Makes the socket reusable. */
int net_socket_make_reuseable_win32(net_socket_fd_t sock);
#define net_socket_make_reuseable(sock) \
        net_socket_make_reuseable_win32(sock)


/** Expands to WSA<b>e</b> on Windows, and to <b>e</b> elsewhere. */
#define NET_SOCKET_ERRNO(e) \
        WSA##e

/** Check whether net_socket_fd_t is not null */
#define NET_SOCKET_OK(s) \
        ((SOCKET)(s) != INVALID_SOCKET)
#define NET_INVALID_SOCKET \
        INVALID_SOCKET

int net_socket_errno(net_socket_fd_t sock);
const char * net_socket_strerror(int e);

/** Return true if e is EAGAIN or the local equivalent. */
#define NET_SOCKET_ERRNO_IS_EAGAIN(e) \
        ((e) == EAGAIN || (e) == WSAEWOULDBLOCK)
/** Return true if e is EINPROGRESS or the local equivalent. */
#define NET_SOCKET_ERRNO_IS_EINPROGRESS(e) \
        ((e) == WSAEINPROGRESS)
/** Return true if e is EINPROGRESS or the local equivalent as returned by
 * a call to connect(). */
#define NET_SOCKET_ERRNO_IS_CONN_EINPROGRESS(e) \
        ((e) == WSAEINPROGRESS || (e)== WSAEINVAL || (e) == WSAEWOULDBLOCK)
/** Return true if e is EAGAIN or another error indicating that a call to
 * accept() has no pending connections to return. */
#define NET_SOCKET_ERRNO_IS_ACCEPT_EAGAIN(e) \
        NET_SOCKET_ERRNO_IS_EAGAIN(e)
/** Return true if e is EMFILE or another error indicating that a call to
 * accept() has failed because we're out of fds or something. */
#define NET_SOCKET_ERRNO_IS_RESOURCE_LIMIT(e) \
        ((e) == WSAEMFILE || (e) == WSAENOBUFS)
/** Return true if e is EADDRINUSE or the local equivalent. */
#define NET_SOCKET_ERRNO_IS_EADDRINUSE(e) \
        ((e) == WSAEADDRINUSE)
/** Return true if e is EINTR  or the local equivalent */
#define NET_SOCKET_ERRNO_IS_EINTR(e) \
        ((e) == WSAEINTR || 0)


/* --- Unix Sockets --- */
#else
/** Makes the socket reusable. */
int net_socket_make_reuseable_unix(net_socket_fd_t sock);
#define net_socket_make_reuseable(sock) \
        net_socket_make_reuseable_unix(sock)

#define NET_SOCKET_ERRNO(e) \
        e
/** Macro: true iff 's' is a possible value for a valid initialized socket. */
#define NET_SOCKET_OK(s) \
        ((s) >= 0)
/** Error/uninitialized value for a tor_socket_t. */
#define NET_INVALID_SOCKET \
        (-1)

/* For Unix based networking, we do not need to implement our own error
 * functions. */
#define net_socket_errno(sock) \
        (errno)
#define net_socket_strerror(e) \
        strerror(e)

#if EAGAIN == EWOULDBLOCK
/* || 0 is for -Wparentheses-equality (-Wall?) appeasement under clang. */
#define NET_SOCKET_ERRNO_IS_EAGAIN(e) \
        ((e) == EAGAIN || 0)
#else
#define NET_SOCKET_ERRNO_IS_EAGAIN(e) \
        ((e) == EAGAIN || (e) == EWOULDBLOCK)
#endif

#define NET_SOCKET_ERRNO_IS_EINTR(e) \
        ((e) == EINTR || 0)
#define NET_SOCKET_ERRNO_IS_EINPROGRESS(e) \
        ((e) == EINPROGRESS || 0)
#define NET_SOCKET_ERRNO_IS_CONN_EINPROGRESS(e) \
        ((e) == EINPROGRESS || 0)
#define NET_SOCKET_ERRNO_IS_ACCEPT_EAGAIN(e) \
        (NET_SOCKET_ERRNO_IS_EAGAIN(e) || (e) == ECONNABORTED)
#define NET_SOCKET_ERRNO_IS_RESOURCE_LIMIT(e) \
        ((e) == EMFILE || (e) == ENFILE || (e) == ENOBUFS || (e) == ENOMEM)
#define NET_SOCKET_ERRNO_IS_EADDRINUSE(e) \
        (((e) == EADDRINUSE) || 0)
#endif
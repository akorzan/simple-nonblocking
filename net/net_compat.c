/** Modified and simplified tor_compat.c */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

/** Headers for net_sockets */
#include "net_util.h"
#include "net_compat.h"



#if defined(_WIN32)
/**
 * On Windows, WSAEWOULDBLOCK is not always correct: when you see it,
 * you need to ask the socket for its actual errno.  Also, you need to
 * get your errors from WSAGetLastError, not errno.
 *
 * If you supply a socket of -1, we check WSAGetLastError, but don't correct
 * WSAEWOULDBLOCKs.
 *
 * The upshot of all of this is that when a socket call fails, you
 * should call net_socket_errno <em>at most once</em> on the failing
 * socket to get the error.
 */
int
net_socket_errno(net_socket_fd_t sock)
{
        int optval;
        int optvallen = sizeof(optval);
        int err = WSAGetLastError();
        // Short circuit if this operation would block on a nonblocking socket.
        if (err == WSAEWOULDBLOCK && NET_SOCKET_OK(sock)) {
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&optval,
                               &optvallen))
                        return err;
                if (optval)
                        return optval;
        }
        return err;
}

#define E(code, s) { code, (s " [" #code " ]") }
struct { int code; const char *msg; } windows_socket_errors[] = {
        E(WSAEINTR, "Interrupted function call"),
        E(WSAEACCES, "Permission denied"),
        E(WSAEFAULT, "Bad address"),
        E(WSAEINVAL, "Invalid argument"),
        E(WSAEMFILE, "Too many open files"),
        E(WSAEWOULDBLOCK,  "Resource temporarily unavailable"),
        E(WSAEINPROGRESS, "Operation now in progress"),
        E(WSAEALREADY, "Operation already in progress"),
        E(WSAENOTSOCK, "Socket operation on nonsocket"),
        E(WSAEDESTADDRREQ, "Destination address required"),
        E(WSAEMSGSIZE, "Message too long"),
        E(WSAEPROTOTYPE, "Protocol wrong for socket"),
        E(WSAENOPROTOOPT, "Bad protocol option"),
        E(WSAEPROTONOSUPPORT, "Protocol not supported"),
        E(WSAESOCKTNOSUPPORT, "Socket type not supported"),
        /* What's the difference between NOTSUPP and NOSUPPORT? :) */
        E(WSAEOPNOTSUPP, "Operation not supported"),
        E(WSAEPFNOSUPPORT,  "Protocol family not supported"),
        E(WSAEAFNOSUPPORT, "Address family not supported by protocol family"),
        E(WSAEADDRINUSE, "Address already in use"),
        E(WSAEADDRNOTAVAIL, "Cannot assign requested address"),
        E(WSAENETDOWN, "Network is down"),
        E(WSAENETUNREACH, "Network is unreachable"),
        E(WSAENETRESET, "Network dropped connection on reset"),
        E(WSAECONNABORTED, "Software caused connection abort"),
        E(WSAECONNRESET, "Connection reset by peer"),
        E(WSAENOBUFS, "No buffer space available"),
        E(WSAEISCONN, "Socket is already connected"),
        E(WSAENOTCONN, "Socket is not connected"),
        E(WSAESHUTDOWN, "Cannot send after socket shutdown"),
        E(WSAETIMEDOUT, "Connection timed out"),
        E(WSAECONNREFUSED, "Connection refused"),
        E(WSAEHOSTDOWN, "Host is down"),
        E(WSAEHOSTUNREACH, "No route to host"),
        E(WSAEPROCLIM, "Too many processes"),
        /* Yes, some of these start with WSA, not WSAE. No, I don't know why. */
        E(WSASYSNOTREADY, "Network subsystem is unavailable"),
        E(WSAVERNOTSUPPORTED, "Winsock.dll out of range"),
        E(WSANOTINITIALISED, "Successful WSAStartup not yet performed"),
        E(WSAEDISCON, "Graceful shutdown now in progress"),
#ifdef WSATYPE_NOT_FOUND
        E(WSATYPE_NOT_FOUND, "Class type not found"),
#endif
        E(WSAHOST_NOT_FOUND, "Host not found"),
        E(WSATRY_AGAIN, "Nonauthoritative host not found"),
        E(WSANO_RECOVERY, "This is a nonrecoverable error"),
        E(WSANO_DATA, "Valid name, no data record of requested type)"),

        /* There are some more error codes whose numeric values are marked
        * <b>OS dependent</b>. They start with WSA_, apparently for the same
        * reason that practitioners of some craft traditions deliberately
        * introduce imperfections into their baskets and rugs "to allow the
        * evil spirits to escape."  If we catch them, then our binaries
        * might not report consistent results across versions of Windows.
        * Thus, I'm going to let them all fall through.
        */
        { -1, NULL },
};

/** 
 * There does not seem to be a strerror equivalent for Winsock errors.
 * Naturally, we have to roll our own.
 */
const char *
net_socket_strerror(int e)
{
        int i;
        for (i=0; windows_socket_errors[i].code >= 0; ++i) {
                if (e == windows_socket_errors[i].code)
                        return windows_socket_errors[i].msg;
        }
        return strerror(e);
}

/**
 * Tell the Windows TCP stack to prevent other applications from receiving
 * traffic from tor's open ports. Return 0 on success, -1 on failure.
 */
int
net_socket_make_exclusive_win32(net_socket_fd_t sock)
{
#ifdef SO_EXCLUSIVEADDRUSE
        int one = 1;

        /* Any socket that sets REUSEADDR on win32 can bind to a port even when
         * somebody else already has it bound, and even if the original socket
         * didn't set REUSEADDR. Use EXCLUSIVEADDRUSE to prevent this
         * port-stealing on win32. */
        if (setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (void*) &one,
                        (socklen_t)sizeof(one))) {
                return -1;
        }
        return 0;
#else
        (void) sock;
        return 0;
#endif
}
/* -- End of Windows specific functions -- */
#else
/* -- Unix equivalents -- */

/**
 * Tell the Unix TCP stack that it shouldn't wait for a long time after
 * <b>sock</b> has closed before reusing its port. Return 0 on success,
 * -1 on failure.
 */
int
net_socket_make_reuseable_unix(net_socket_t sock)
{
// TODO: Check if tor uses these options on every socket including 1024+
        int one = 1;

        /* REUSEADDR on normal places means you can rebind to the port
         * right after somebody else has let it go. But REUSEADDR on win32
         * means you can bind to the port _even when somebody else
         * already has it bound_. So, don't do that on Win32. */
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &one,
                        (socklen_t) sizeof(one)) == -1) {
                return -1;
        }
        return 0;
}
#endif

/**
 * Turn <b>socket</b> into a nonblocking socket.
 * Return 0 on success, -1 on failure.
 */
int
_net_socket_set_nonblocking(net_socket_fd_t sock)
{
#if defined(_WIN32)
        unsigned long nonblocking = 1;
        ioctlsocket(sock, FIONBIO, (unsigned long*) &nonblocking);
#else
        int flags;

        flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) {
                net_warn("Couldn't get file status flags: %s.\n",
                         net_socket_strerror(errno));
                return -1;
        }
        flags |= O_NONBLOCK;
        if (fcntl(sock, F_SETFL, flags) == -1) {
                net_warn("Couldn't set file status flags: %s.\n",
                         net_socket_strerror(errno));
                return -1;
        }
#endif
        return 0;
}


/**
 * As socket().
 *
 * <b>cloexec</b> and <b>nonblock</b> should be either 0 or 1 to indicate
 * if the corresponding extension, SOCK_CLOEXEC or SOCK_NONBLOCK, should be used.
 *
 * Returns net_socket_fd_t, a macro to an int that holds socket fds.
 *
 * Returns -1 if an error occurred.  Use the macro net_socket_errno to
 * read ernno.
 */
net_socket_fd_t
_net_socket(int domain, int type, int protocol, int cloexec, int nonblock)
{
        net_socket_fd_t s;
        /*
         * When using non-blocking, also set SOCK_CLOEXEC.
         *
         * From the manual page for open(2)
         * Note that the use of the FD_CLOEXEC flag is essential in some
         * multithreaded programs, because using a separate fcntl(2)
         * F_SETFD operation to set the FD_CLOEXEC flag does not suffice
         * to avoid race conditions where one thread opens a file
         * descriptor and attempts to set its close-on-exec flag using
         * fcntl(2) at the same time as another thread does a fork(2)
         * plus execve(2). */

        // Macros handle different systems implementations of networking.
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
        int ext_flags = (cloexec ? SOCK_CLOEXEC : 0) |
                        (nonblock ? SOCK_NONBLOCK : 0);
        s = socket(domain, type|ext_flags, protocol);

        /* A macro for checking if the socket file descriptor is not null */
        if ( NET_SOCKET_OK(s) )
                goto net_socket_open_ok;
        /* If we got an error, see if it is EINVAL. EINVAL might indicate that,
         * even though we were built on a system with SOCK_CLOEXEC and
         * SOCK_NONBLOCK support, we are running on one without. */
        if (errno != EINVAL)
                return s;

#endif /* SOCK_CLOEXEC && SOCK_NONBLOCK */

        s = socket(domain, type, protocol);
        if (! NET_SOCKET_OK(s))
                return s;

#if defined(FD_CLOEXEC)
        if (cloexec) {
                if (fcntl(s, F_SETFD, FD_CLOEXEC) == -1) {
                        net_warn("Couldn't set FD_CLOEXEC: %s.\n",
                                  net_socket_strerror(errno));
                        /* Socket was not yet logged, use normal close. */
                        net_close(s);
                        /* A macro for failure; returns -1 on unix,
                         * or INVALID_SOCKET on windows. */
                        return NET_INVALID_SOCKET;
                }
        }
#else
        // This is a no-op, and nothing happens.  Possibly to avoid warnings?
        (void) cloexec;
#endif

        if (nonblock) {
                if (_net_socket_set_nonblocking(s) == -1) {
                        net_close(s);
                        return NET_INVALID_SOCKET;
                }
        }

        goto net_socket_open_ok;

net_socket_open_ok:
        return s;
}


/**
 * As socket(), but creates a blocking socket.
 * 
 * Returns net_socket_fd_t, a macro to an int that is used to hold socket fds.
 * Returns -1 if an error occurred.  Use the macro net_socket_errno to read
 * ernno. */
net_socket_fd_t
net_socket_blocking(int domain, int type, int protocol)
{
        return _net_socket(domain, type, protocol, 1, 0);
}
        

/**
 * As socket(), but creates a nonblocking socket.
 * 
 * Returns net_socket_fd_t, a macro to an int that is used to hold socket fds.
 * Returns -1 if an error occurred.  Use the macro net_socket_errno to read
 * ernno. */
net_socket_fd_t
net_socket_nonblocking(int domain, int type, int protocol)
{
        return _net_socket(domain, type, protocol, 1, 1);
}


/**
 * A cross-platform wrapper for closing a socket regardless of flags.
 *
 * As close(), but guaranteed to work for sockets across platforms (including
 * Windows, where close()ing a socket doesn't work.
 *
 * Returns 0 on success and the socket error code on failure. */
int
net_close(net_socket_fd_t sock_fd)
{
        int r = 0;

        /* On Windows, you have to call close() on fds returned by open(),
        * and closesocket() on fds returned by socket().  On Unix, everything
        * gets close()'d. */
#if defined(_WIN32)
        r = closesocket(sock_fd);
#else
        r = close(sock_fd);
#endif

        if (r != 0) {
                // Macros that return errno and strerror resepectively on Unix.
                int err = net_socket_errno(-1);
                net_warn("Close returned an error: %s.\n",
                          net_socket_strerror(err));
                return err;
        }

        return r;
}


/**
 * As accept(), but handles socket creation with either of SOCK_CLOEXEC
 * and SOCK_NONBLOCK specified.
 *
 * <b>cloexec</b> and <b>nonblock</b> should be either 0 or 1 to indicate
 * if the corresponding extension should be used.
 */
net_socket_fd_t
_net_accept(net_socket_fd_t sock_fd, struct sockaddr *addr,
            socklen_t *len, int cloexec, int nonblock)
{
        /* The socket returned and connected to the new client. */
        net_socket_fd_t client_fd;

#if defined(HAVE_ACCEPT4) && defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
        int ext_flags = (cloexec ? SOCK_CLOEXEC : 0) |
                  (nonblock ? SOCK_NONBLOCK : 0);
        client_fd = accept4(sock_fd, addr, len, ext_flags);
        
        if (NET_SOCKET_OK(client_fd))
                return client_fd;
        /* If we got an error, see if it is ENOSYS. ENOSYS indicates that,
        * even though we were built on a system with accept4 support, we
        * are running on one without. Also, check for EINVAL, which indicates
        * that we are missing SOCK_CLOEXEC/SOCK_NONBLOCK support. */
        if (errno != EINVAL && errno != ENOSYS)
                return client_fd;
        /* Try again using the regular accept(). */
#endif

        client_fd = accept(sock_fd, addr, len);
        /* If error return error code. */
        if ( ! NET_SOCKET_OK(client_fd) )
                return client_fd;

#if defined(FD_CLOEXEC)
        if (cloexec) {
                if (fcntl(client_fd, F_SETFD, FD_CLOEXEC) == -1) {
                        net_warn( "Couldn't set FD_CLOEXEC: %s.\n",
                                  strerror(errno) );
                        net_close(client_fd);
                        return NET_INVALID_SOCKET;
                }
        }
#else
        // no operation (no-op), possibly to remove a warning; there's
        // no other reason I can come up with.
        (void)cloexec;
#endif

        if (nonblock) {
                if (_net_socket_set_nonblocking(client_fd) == -1) {
                        net_close(client_fd);
                        return NET_INVALID_SOCKET;
                }
        }

        return client_fd;
}

/**
 * As accept(); creates a new blocking socket.
 * 
 * Returns net_socket_fd_t, a macro to an int that is used to hold socket fds.
 * Returns -1 if an error occurred.  Use the macro net_socket_errno to read
 * ernno. */
net_socket_fd_t
net_accept_blocking(net_socket_fd_t sock_fd, struct sockaddr *addr,
                    socklen_t *len)
{
        return _net_accept(sock_fd, addr, len, 1, 0);
}
        

/**
 * As accept(), but creates a new nonblocking socket.
 * 
 * Returns net_socket_fd_t, a macro to an int that is used to hold socket fds.
 * Returns -1 if an error occurred.  Use the macro net_socket_errno to read
 * ernno. */
net_socket_fd_t
net_accept_nonblocking(net_socket_fd_t sock_fd, struct sockaddr *addr,
                       socklen_t *len)
{
        return _net_accept(sock_fd, addr, len, 1, 1);
}


/* As write(), but retry on EINTR, and return the negative error code on
 * error. */
static int
write_ni(int fd, const void *buf, size_t n)
{
  int r;
 again:
  r = (int) write(fd, buf, n);
  if (r < 0) {
    if (errno == EINTR)
      goto again;
    else
      return -errno;
  }
  return r;
}
/* As read(), but retry on EINTR, and return the negative error code on error.
 */
static int
read_ni(int fd, void *buf, size_t n)
{
  int r;
 again:
  r = (int) read(fd, buf, n);
  if (r < 0) {
    if (errno == EINTR)
      goto again;
    else
      return -errno;
  }
  return r;
}
#endif

/** As send(), but retry on EINTR, and return the negative error code on
 * error. */
static int
send_ni(int fd, const void *buf, size_t n, int flags)
{
  int r;
 again:
  r = (int) send(fd, buf, n, flags);
  if (r < 0) {
    int error = tor_socket_errno(fd);
    if (ERRNO_IS_EINTR(error))
      goto again;
    else
      return -error;
  }
  return r;
}

/** As recv(), but retry on EINTR, and return the negative error code on
 * error. */
static int
recv_ni(int fd, void *buf, size_t n, int flags)
{
  int r;
 again:
  r = (int) recv(fd, buf, n, flags);
  if (r < 0) {
    int error = tor_socket_errno(fd);
    if (ERRNO_IS_EINTR(error))
      goto again;
    else
      return -error;
  }
  return r;
}

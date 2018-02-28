/* These attributes are local to the server.  I have placed them into a struct,
 * to allow multiple instances of a server in one process. */
struct _server_vars {
        struct ring *rb;
        pthread_mutex_t rb_mutex_sum;
        pthread_cond_t rb_not_full;
        pthread_cond_t rb_not_empty;

        /* The least significant bit marks the run boolean. */
        unsigned int flags;
};


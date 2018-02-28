#ifndef _NET_UTIL_H
#define _NET_UTIL_H
#endif

void net_debug(const char* format, ...);
void net_print(const char* format, ...);
void net_warn(const char* format, ...);
void net_error(const char* format, ...);

#if defined(__GNUC__) && __GNUC__ >= 3
/** Macro: Evaluates to <b>exp</b> and hints the compiler that the value
 * of <b>exp</b> will probably be true.
 *
 * In other words, "if (PREDICT_LIKELY(foo))" is the same as "if (foo)",
 * except that it tells the compiler that the branch will be taken most of the
 * time.  This can generate slightly better code with some CPUs.
 */
#define PREDICT_LIKELY(exp) __builtin_expect(!!(exp), 1)
/** Macro: Evaluates to <b>exp</b> and hints the compiler that the value
 * of <b>exp</b> will probably be false.
 *
 * In other words, "if (PREDICT_UNLIKELY(foo))" is the same as "if (foo)",
 * except that it tells the compiler that the branch will usually not be
 * taken.  This can generate slightly better code with some CPUs.
 */
#define PREDICT_UNLIKELY(exp) __builtin_expect(!!(exp), 0)
#else
#define PREDICT_LIKELY(exp) (exp)
#define PREDICT_UNLIKELY(exp) (exp)
#endif

#ifdef __GNUC__
/** STMT_BEGIN and STMT_END are used to wrap blocks inside macros so that
 * the macro can be used as if it were a single C statement. */
#define STMT_BEGIN (void) ({
#define STMT_END })
#elif defined(sun) || defined(__sun__)
#define STMT_BEGIN if (1) {
#define STMT_END } else (void)0
#else
#define STMT_BEGIN do {
#define STMT_END } while (0)
#endif

#define net_assert(expr) STMT_BEGIN \
        if (PREDICT_UNLIKELY(!(expr))) {                                \
                net_error("%s:%d: %s: Assertion %s failed; aborting\n", \
                          _SHORT_FILE_, __LINE__, __func__, #expr);     \
                abort();                                                \
        }                                                               \
STMT_END

#define net_assert_nonfatal(expr) STMT_BEGIN \
        if (PREDICT_UNLIKELY(!(cond))) {                                       \
                net_error("%s:%d: %s: Bug has occured; expression %s failed\n",\
                          _SHORT_FILE_, __LINE__, __func__, #expr);            \
        }                                                                      \
STMT_END


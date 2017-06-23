#ifndef _FLUX_CORE_KVS_H
#define _FLUX_CORE_KVS_H

#include <stdbool.h>
#include <stdint.h>
#include <flux/core.h>

#include "kvs_lookup.h"
#include "kvs_dir.h"
#include "kvs_classic.h"
#include "kvs_watch.h"
#include "kvs_txn.h"

/* kvs_put() and kvs_put_string() both make copies of the value argument
 * The caller retains ownership of the original.
 * These functions return -1 on error (errno set), 0 on success.
 */
int kvs_put (flux_t *h, const char *key, const char *json_str);
int kvs_put_string (flux_t *h, const char *key, const char *val);
int kvs_put_int (flux_t *h, const char *key, int val);
int kvs_put_int64 (flux_t *h, const char *key, int64_t val);
int kvs_put_double (flux_t *h, const char *key, double val);
int kvs_put_boolean (flux_t *h, const char *key, bool val);

/* As above but associate a preconstructed treeobj with key.
 */
int kvs_put_treeobj (flux_t *h, const char *key, const char *treeobj);

/* Remove a key from the namespace.  If it represents a directory,
 * its contents are also removed.  kvsdir_unlink removes it relative to 'dir'.
 * Since ordering of put/mkdir/unlink requests within a commit is not defined,
 * the idiom: "unlink a; put a.a; put a.b; commit" should be avoided.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_unlink (flux_t *h, const char *key);

/* Create symlink.  kvsdir_symlink creates it relatived to 'dir'.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_symlink (flux_t *h, const char *key, const char *target);

/* Create an empty directory.  kvsdir_mkdir creates it relative to 'dir'.
 * Usually this is not necessary, as kvs_put() will create directories
 * for path components if they do not exist.
 * A kvs_mkdir() of an existing directory is not an error.
 * A kvs_mkdir() may replace a populated directory with an empty one.
 * Since ordering of put/mkdir/unlink requests within a commit is not defined,
 * the idiom: "mkdir a; put a.a; put a.b; commit" should be avoided.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_mkdir (flux_t *h, const char *key);

/* kvs_commit() must be called after kvs_put*, kvs_unlink, and kvs_mkdir
 * to finalize the update.  The new data is immediately available on
 * the calling node when the commit returns.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_commit (flux_t *h, int flags);

/* kvs_commit_begin() sends the commit request and returns immediately.
 * kvs_commit_finish() blocks until the response is received, then returns.
 * Use flux_future_then() to arrange for the commit to complete asynchronously.
 */
flux_future_t *kvs_commit_begin (flux_t *h, int flags);
int kvs_commit_finish (flux_future_t *f);

/* kvs_fence() is a collective commit operation.  nprocs tasks make the
 * call with identical arguments.  It is internally optimized to minimize
 * the work that needs to be done.  Once the call returns, all changes
 * from participating tasks are available to all tasks.
 * Note:  if operations were associated with the named fence using
 * kvs_fence_set_context(), only those operations become part of the fence;
 * otherwise fence behavior is like kvs_commit(): all anonymous operations
 * queued in the handle become part of the fence.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_fence (flux_t *h, const char *name, int nprocs, int flags);

/* kvs_fence_begin() sends the fence request and returns immediately.
 * kvs_fence_finish() blocks until the response is received, then returns.
 * Use flux_future_then() to arrange for the fence to complete asynchronously.
 */
flux_future_t *kvs_fence_begin (flux_t *h, const char *name,
                                int nprocs, int flags);
int kvs_fence_finish (flux_future_t *f);

/* Operations (put, unlink, symlink, mkdir) may be associated with a named
 * fence by setting the fence context to that name before issuing them.
 * When the fence context is clear (the default), operations are anonymous.
 */
void kvs_fence_set_context (flux_t *h, const char *name);
void kvs_fence_clear_context (flux_t *h);

/* Synchronization:
 * Process A commits data, then gets the store version V and sends it to B.
 * Process B waits for the store version to be >= V, then reads data.
 */
int kvs_get_version (flux_t *h, int *versionp);
int kvs_wait_version (flux_t *h, int version);

/* Garbage collect the cache.  On the root node, drop all data that
 * doesn't have a reference in the namespace.  On other nodes, the entire
 * cache is dropped and will be reloaded on demand.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_dropcache (flux_t *h);

/* kvsdir_ convenience functions
 * They behave exactly like their kvs_ counterparts, except the 'key' path
 * is resolved relative to the directory.
 */
int kvsdir_put (kvsdir_t *dir, const char *key, const char *json_str);
int kvsdir_put_string (kvsdir_t *dir, const char *key, const char *val);
int kvsdir_put_int (kvsdir_t *dir, const char *key, int val);
int kvsdir_put_int64 (kvsdir_t *dir, const char *key, int64_t val);
int kvsdir_put_double (kvsdir_t *dir, const char *key, double val);
int kvsdir_put_boolean (kvsdir_t *dir, const char *key, bool val);

int kvsdir_unlink (kvsdir_t *dir, const char *key);
int kvsdir_symlink (kvsdir_t *dir, const char *key, const char *target);
int kvsdir_mkdir (kvsdir_t *dir, const char *key);

/* Copy/move a key to a new name.  If 'from' is a directory, copy recursively.
 */
int kvs_copy (flux_t *h, const char *from, const char *to);
int kvs_move (flux_t *h, const char *from, const char *to);

#endif /* !_FLUX_CORE_KVS_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

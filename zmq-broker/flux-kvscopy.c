/* flux-kvscopy.c - copy file to/from KVS */

#define _GNU_SOURCE
#include <getopt.h>
#include <json/json.h>
#include <assert.h>
#include <libgen.h>

#include "cmb.h"
#include "util.h"
#include "log.h"

#define OPTIONS "h"
static const struct option longopts[] = {
    {"help",       no_argument,        0, 'h'},
    { 0, 0, 0, 0 },
};

static int write_all (int fd, uint8_t *buf, int len);
static int read_all (int fd, uint8_t **bufp);

void usage (void)
{
    fprintf (stderr, 
"Usage: flux-copy src dst\n"
"Content stored in the KVS will be z85-encoded.\n"
"src and dst can be:\n"
"       \"-\"                     stdin/stdout\n"
"       name including \"/\"      file\n"
"       (default)               KVS key\n"
);
    exit (1);
}

int main (int argc, char *argv[])
{
    flux_t h;
    int ch, len;
    char *src, *dst;
    uint8_t *buf;

    log_init ("flux-kvscopy");

    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage ();
                break;
            default:
                usage ();
                break;
        }
    }
    if (optind != argc - 2)
        usage ();
    src = argv[optind++];
    dst = argv[optind++];

    if (!(h = cmb_init ()))
        err_exit ("cmb_init");

    /* Read src into memory.
     */
    if (strchr (src, '/')) {
        int fd;
        if ((fd = open (src, O_RDONLY)) < 0)
            err_exit ("open %s", src);
        if ((len = read_all (fd, &buf)) < 0)
            err_exit ("read %s", src);
        (void)close (fd);
    } else if (!strcmp (src, "-")) {
        if ((len = read_all (STDIN_FILENO, &buf)) < 0)
            err_exit ("read %s", src);
    } else { 
        json_object *o;
        if (kvs_get (h, src, &o) < 0)
            err_exit ("kvs_get %s", src);
        if (util_json_object_get_data (o, "data", &buf, &len) < 0)
            err_exit ("%s: JSON decode error", src);
    }

    /* Write memory to dst.
     */
    if (strchr (dst, '/')) {
        int fd;
        if ((fd = creat (dst, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
            err_exit ("creat %s", dst);
        if (write_all (fd, buf, len) < 0)
            err_exit ("write %s", dst);
        if (close (fd) < 0)
            err_exit ("close %s", dst);
    } else if (!strcmp (dst, "-")) {
        if (write_all (STDOUT_FILENO, buf, len) < 0)
            err_exit ("write %s", dst);
    } else {
        json_object *o = util_json_object_new_object ();
        util_json_object_add_data (o, "data", buf, len);
        if (kvs_put (h, dst, o) < 0)
            err_exit ("kvs_put %s", dst);
        json_object_put (o);
        if (kvs_commit (h) < 0)
            err_exit ("kvs_commit");
    }

    free (buf);

    flux_handle_destroy (&h);
    log_fini ();
    return 0;
}

static int write_all (int fd, uint8_t *buf, int len)
{
    int n;
    int count = 0;

    while (count < len) {
        if ((n = write (fd, buf + count, len - count)) < 0)
            return n;
        count += n;
    }
    return count;
}

static int read_all (int fd, uint8_t **bufp)
{
    const int chunksize = 4096;
    int len = 0;
    uint8_t *buf = NULL;
    int n;
    int count = 0;

    do {
        if (len - count == 0) {
            len += chunksize;
            if (!(buf = buf ? realloc (buf, len) : malloc (len)))
                goto nomem;
        }
        if ((n = read (fd, buf + count, len - count)) < 0) {
            free (buf);
            return n;
        }
        count += n;
    } while (n != 0);
    *bufp = buf;
    return count;
nomem:
    errno = ENOMEM;
    return -1;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

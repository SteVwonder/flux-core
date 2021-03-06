/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <argz.h>

#include "pmi.h"
#include "clique.h"

static int catprintf (char **buf, int *bufsz, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start (ap, fmt);
    n = vsnprintf (*buf, *bufsz, fmt, ap);
    va_end (ap);
    if (n >= *bufsz)
        return -1;
    *bufsz -= n;
    *buf += n;
    return 0;
}

int pmi_process_mapping_encode (struct pmi_map_block *blocks,
                                int nblocks,
                                char *buf,
                                int bufsz)
{
    int i;

    if (catprintf (&buf, &bufsz, "(vector,") < 0)
        return -1;
    for (i = 0; i < nblocks; i++) {
        if (catprintf (&buf, &bufsz, "%s(%d,%d,%d)",
                       i > 0 ? "," : "",
                       blocks[i].nodeid,
                       blocks[i].nodes,
                       blocks[i].procs) < 0)
            return -1;
    }
    if (catprintf (&buf, &bufsz, ")") < 0)
        return -1;
    return 0;
}

static int parse_block (const char *s, struct pmi_map_block *block)
{
    char *endptr;

    errno = 0;
    block->nodeid = strtoul (s, &endptr, 10);
    if (errno != 0 || *endptr != ',')
        return PMI_FAIL;
    s = endptr + 1;
    errno = 0;
    block->nodes = strtoul (s, &endptr, 10);
    if (errno != 0 || *endptr != ',')
        return PMI_FAIL;
    s = endptr + 1;
    errno = 0;
    block->procs = strtoul (s, &endptr, 10);
    if (errno != 0 || *endptr != ')')
        return PMI_FAIL;
    return PMI_SUCCESS;
}

int pmi_process_mapping_parse (const char *s,
                               struct pmi_map_block **blocksp, int *nblocksp)
{
    char *argz = NULL;
    size_t argz_len;
    char *entry = NULL;
    int nblocks, i;
    struct pmi_map_block *blocks = NULL;
    int rc = PMI_FAIL;

    /* Special case empty string.
     * Not an error: return no blocks.
     */
    if (*s == '\0') {
        *blocksp = NULL;
        *nblocksp = 0;
        return PMI_SUCCESS;
    }

    if (argz_create_sep (s, '(', &argz, &argz_len) != 0) {
        rc = PMI_ERR_NOMEM;
        goto error;
    }
    nblocks = argz_count (argz, argz_len);
    while ((entry = argz_next (argz, argz_len, entry))) {
        nblocks--;
        if (strstr (entry, "vector,"))
            break;
    }
    if (nblocks == 0)
        goto error;
    if (!(blocks = calloc (nblocks, sizeof (blocks[0])))) {
        rc = PMI_ERR_NOMEM;
        goto error;
    }
    i = 0;
    while ((entry = argz_next (argz, argz_len, entry))) {
        if ((rc = parse_block (entry, &blocks[i++])) != PMI_SUCCESS)
            goto error;
    }
    *nblocksp = nblocks;
    *blocksp = blocks;
    free (argz);
    return PMI_SUCCESS;
error:
    if (blocks)
        free (blocks);
    if (argz)
        free (argz);
    return rc;
}

int pmi_process_mapping_find_nodeid (struct pmi_map_block *blocks, int nblocks,
                                     int rank, int *nodeid)
{
    int i;
    int brank = 0;

    for (i = 0; i < nblocks; i++) {
        int lsize = blocks[i].nodes * blocks[i].procs;
        int lrank = rank - brank;

        if (lrank >= 0 && lrank < lsize) {
            *nodeid = blocks[i].nodeid + lrank / blocks[i].procs;
            return PMI_SUCCESS;
        }
        brank += lsize;
    }
    return PMI_FAIL;
}

int pmi_process_mapping_find_nranks (struct pmi_map_block *blocks, int nblocks,
                                     int nodeid, int size, int *nranksp)
{
    int i, j;
    int brank = 0, nranks = 0;

    for (i = 0; i < nblocks; i++) {
        for (j = 0; j < blocks[i].nodes; j++) {
            int lsize = blocks[i].procs;
            if (brank + lsize > size)
                lsize -= (brank - size);
            if (blocks[i].nodeid + j == nodeid)
                nranks += lsize;
            brank += lsize;
        }
    }
    *nranksp = nranks;
    return PMI_SUCCESS;
}

int pmi_process_mapping_find_ranks (struct pmi_map_block *blocks, int nblocks,
                                    int nodeid, int size,
                                    int *ranks, int nranks)
{
    int i, j, k, nx = 0;
    int brank = 0;

    for (i = 0; i < nblocks; i++) {
        for (j = 0; j < blocks[i].nodes; j++) {
            int lsize = blocks[i].procs;
            if (brank + lsize > size)
                lsize -= (brank - size);
            if (blocks[i].nodeid + j == nodeid) {
                for (k = 0; k < lsize; k++) {
                    if (nx >=  nranks)
                        return PMI_ERR_INVALID_SIZE;
                    ranks[nx++] = brank + k;
                }
            }
            brank += lsize;
        }
    }
    if (nx != nranks)
        return PMI_ERR_INVALID_SIZE;
    return PMI_SUCCESS;
}

char *pmi_cliquetostr (char *buf, int bufsz, int *ranks, int length)
{
    int n, i, count;

    buf[0] = '\0';
    for (i = 0, count  = 0; i < length; i++) {
        n = snprintf (buf + count,
                      bufsz - count,
                      "%s%d",
                      i > 0 ? "," : "",
                      ranks[i]);
        if (n >= bufsz - count)
            return "overflow";
        count += n;
    }
    return buf;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

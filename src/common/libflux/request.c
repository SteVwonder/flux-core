/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "request.h"
#include "message.h"
#include "info.h"

#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/xzmalloc.h"
#include "src/common/libutil/nodeset.h"

int flux_json_request_decode (zmsg_t *zmsg, json_object **in)
{
    int type;
    int rc = -1;
    JSON o;

    if (in == NULL || zmsg == NULL) {
        errno = EINVAL;
        goto done;
    }
    if (flux_msg_get_type (zmsg, &type) < 0)
        goto done;
    if (type != FLUX_MSGTYPE_REQUEST) {
        errno = EPROTO;
        goto done;
    }
    if (flux_msg_get_payload_json (zmsg, &o) < 0)
        goto done;
    if (o == NULL) {
        errno = EPROTO;
        goto done;
    }
    *in = o;
    rc = 0;
done:
    return rc;
}

int flux_json_response_decode (zmsg_t *zmsg, json_object **out)
{
    int errnum;
    JSON o;
    int rc = -1;

    if (out == NULL || zmsg == NULL) {
        errno = EINVAL;
        goto done;
    }
    if (flux_msg_get_errnum (zmsg, &errnum) < 0)
        goto done;
    if (errnum != 0) {
        errno = errnum;
        goto done;
    }
    if (flux_msg_get_payload_json (zmsg, &o) < 0)
        goto done;
    if (o == NULL) {
        errno = EPROTO;
        goto done;
    }
    *out = o;
    rc = 0;
done:
    return rc;
}

int flux_response_decode (zmsg_t *zmsg)
{
    int rc = -1;
    int errnum;

    if (zmsg == NULL) {
        errno = EINVAL;
        goto done;
    }
    if (flux_msg_get_errnum (zmsg, &errnum) < 0)
        goto done;
    if (errnum != 0) {
        errno = errnum;
        goto done;
    }
    if (flux_msg_has_payload (zmsg)) {
        errno = EPROTO;
        goto done;
    }
    rc = 0;
done:
    return rc;
}

int flux_json_request (flux_t h, uint32_t nodeid, uint32_t matchtag,
                       const char *topic, JSON in)
{
    zmsg_t *zmsg;
    int rc = -1;

    if (!topic) {
        errno = EINVAL;
        goto done;
    }
    if (!(zmsg = flux_msg_create (FLUX_MSGTYPE_REQUEST)))
        goto done;
    if (flux_msg_set_nodeid (zmsg, nodeid) < 0)
        goto done;
    if (flux_msg_set_matchtag (zmsg, matchtag) < 0)
        goto done;
    if (flux_msg_set_topic (zmsg, topic) < 0)
        goto done;
    if (flux_msg_set_payload_json (zmsg, in) < 0)
        goto done;
    if (flux_msg_enable_route (zmsg) < 0)
        goto done;
    rc = flux_request_sendmsg (h, &zmsg);
done:
    zmsg_destroy (&zmsg);
    return rc;
}

/* helper for flux_json_multrpc */
static int multrpc_cb (zmsg_t *zmsg, uint32_t nodeid,
                       flux_multrpc_f cb, void *arg)
{
    int errnum = 0;
    JSON out = NULL;

    if (flux_msg_get_errnum (zmsg, &errnum) < 0)
        errnum = errno;
    if (errnum == 0 && flux_msg_get_payload_json (zmsg, &out) < 0)
        errnum = errno;
    if (cb && cb (nodeid, errnum, out, arg) < 0)
        errnum = errno;
    Jput (out);
    if (errnum) {
        errno = errnum;
        return -1;
    }
    return 0;
}

int flux_json_multrpc (flux_t h, const char *nodeset, int fanout,
                       const char *topic, json_object *in,
                       flux_multrpc_f cb, void *arg)
{
    nodeset_t ns = nodeset_new_str (nodeset);
    uint32_t maxtag, mintag = FLUX_MATCHTAG_NONE;
    nodeset_itr_t itr;
    int errnum = 0;
    int count = 0;
    uint32_t *nodeids = NULL;
    zlist_t *nomatch = NULL;
    int ntx, nrx, i;

    if (!(nomatch = zlist_new ()))
        oom ();
    if (!ns || nodeset_max (ns) >= flux_size (h)) {
        errnum = EINVAL;
        goto done;
    }
    count = nodeset_count (ns);

    /* Allocate block of matchtags.
     */
    mintag = flux_matchtag_alloc (h, count);
    if (mintag == FLUX_MATCHTAG_NONE) {
        errnum = EAGAIN;
        goto done;
    }
    maxtag = mintag + count - 1;

    /* Build map of matchtag -> nodeid
     */
    nodeids = xzmalloc (count * sizeof (nodeids[0]));
    if (!(itr = nodeset_itr_new (ns)))
        oom ();
    for (i = 0; i < count; i++)
        nodeids[i] = nodeset_next (itr);
    nodeset_itr_destroy (itr);

    /* Keep 'fanout' requests active concurrently
     */
    ntx = nrx = 0;
    while (ntx < count || nrx < count) {
        while (ntx < count && ntx - nrx < fanout) {
            uint32_t tag = mintag + ntx;
            uint32_t nodeid = nodeids[ntx++];

            if (flux_json_request (h, nodeid, tag, topic, in) < 0) {
                if (errnum < errno)
                    errnum = errno;
                if (cb)
                    cb (nodeid, errno, NULL, arg);
                nrx++;
            }
        }
        while (nrx < count && (ntx - nrx == fanout || ntx == count)) {
            uint32_t tag;
            uint32_t nodeid;
            zmsg_t *zmsg;

            if (!(zmsg = flux_response_recvmsg (h, FLUX_MATCHTAG_NONE, false))
                    || flux_msg_get_matchtag (zmsg, &tag) < 0) {
                zmsg_destroy (&zmsg);
                continue;
            }
            if (tag < mintag || tag > maxtag) {
                if (zlist_append (nomatch, zmsg) < 0)
                    oom ();
                continue;
            }
            nodeid = nodeids[tag - mintag];
            if (multrpc_cb (zmsg, nodeid, cb, arg) < 0) {
                if (errnum < errno)
                    errnum = errno;
            }
            zmsg_destroy (&zmsg);
            nrx++;
        }
    }

    /* Return alien responses to handle.
     */
    if (zlist_size (nomatch) > 0) {
        zmsg_t *zmsg;
        while ((zmsg = zlist_pop (nomatch))) {
            if (flux_response_putmsg (h, &zmsg) < 0)
                zmsg_destroy (&zmsg);
        }
    }
done:
    if (nodeids)
        free (nodeids);
    if (mintag != FLUX_MATCHTAG_NONE)
        flux_matchtag_free (h, mintag, count);
    if (ns)
        nodeset_destroy (ns);
    if (nomatch)
        zlist_destroy (&nomatch);
    if (errnum)
        errno = errnum;
    return errnum ? -1 : 0;
}

int flux_json_rpc (flux_t h, uint32_t nodeid, const char *topic,
                   JSON in, JSON *out)
{
    zmsg_t *zmsg = NULL;
    int rc = -1;
    int errnum;
    JSON o;
    uint32_t matchtag = flux_matchtag_alloc (h, 1);

    if (matchtag == FLUX_MATCHTAG_NONE) {
        errno = EAGAIN;
        goto done;
    }
    if (flux_json_request (h, nodeid, matchtag, topic, in) < 0)
        goto done;
    if (!(zmsg = flux_response_recvmsg (h, matchtag, false)))
        goto done;
    if (flux_msg_get_errnum (zmsg, &errnum) < 0)
        goto done;
    if (errnum != 0) {
        errno = errnum;
        goto done;
    }
    if (flux_msg_get_payload_json (zmsg, &o) < 0)
        goto done;
    /* In order to support flux_rpc(), which in turn must support no-payload
     * responses, this cannot be an error yet.
     */
    if ((!o && out)) {
        *out = NULL;
        //errno = EPROTO;
        //goto done;
    }
    if ((o && !out)) {
        Jput (o);
        errno = EPROTO;
        goto done;
    }
    if (out)
        *out = o;
    rc = 0;
done:
    flux_matchtag_free (h, matchtag, 1);
    return rc;
}

int flux_json_respond (flux_t h, JSON out, zmsg_t **zmsg)
{
    int rc = -1;

    if (flux_msg_set_type (*zmsg, FLUX_MSGTYPE_RESPONSE) < 0)
        goto done;
    if (flux_msg_set_payload_json (*zmsg, out) < 0)
        goto done;
    rc = flux_response_sendmsg (h, zmsg);
done:
    return rc;
}

int flux_err_respond (flux_t h, int errnum, zmsg_t **zmsg)
{
    int rc = -1;
    if (flux_msg_set_type (*zmsg, FLUX_MSGTYPE_RESPONSE) < 0)
        goto done;
    if (flux_msg_set_errnum (*zmsg, errnum) < 0)
        goto done;
    if (flux_msg_set_payload_json (*zmsg, NULL) < 0)
        goto done;
    rc = flux_response_sendmsg (h, zmsg);
done:
    return rc;
}

/**
 ** Deprecated functions.
 */

int flux_respond (flux_t h, zmsg_t **zmsg, JSON o)
{
    return flux_json_respond (h, o, zmsg);
}

int flux_respond_errnum (flux_t h, zmsg_t **zmsg, int errnum)
{
    return flux_err_respond (h, errnum, zmsg);
}

JSON flux_rank_rpc (flux_t h, int rank, JSON o, const char *fmt, ...)
{
    uint32_t nodeid = rank == -1 ? FLUX_NODEID_ANY : rank;
    va_list ap;
    char *topic;
    JSON out;
    int rc;

    va_start (ap, fmt);
    topic = xvasprintf (fmt, ap);
    va_end (ap);

    rc = flux_json_rpc (h, nodeid, topic, o, &out);
    free (topic);
    return rc < 0 ? NULL : out;
}

int flux_rank_request_send (flux_t h, int rank, JSON o, const char *fmt, ...)
{
    uint32_t nodeid = (rank == -1 ? FLUX_NODEID_ANY : rank);
    va_list ap;
    char *topic;
    int rc;

    va_start (ap, fmt);
    topic = xvasprintf (fmt, ap);
    va_end (ap);

    rc = flux_json_request (h, nodeid, FLUX_MATCHTAG_NONE, topic, o);
    free (topic);
    return rc;
}

JSON flux_rpc (flux_t h, JSON o, const char *fmt, ...)
{
    va_list ap;
    JSON out;
    char *topic;
    int rc;

    va_start (ap, fmt);
    topic = xvasprintf (fmt, ap);
    va_end (ap);

    rc = flux_json_rpc (h, FLUX_NODEID_ANY, topic, o, &out);
    free (topic);
    return rc < 0 ? NULL : out;
}

int flux_request_send (flux_t h, JSON o, const char *fmt, ...)
{
    va_list ap;
    char *topic;
    int rc;

    va_start (ap, fmt);
    topic = xvasprintf (fmt, ap);
    va_end (ap);

    rc = flux_json_request (h, FLUX_NODEID_ANY, FLUX_MATCHTAG_NONE, topic, o);
    free (topic);
    return rc;
}

int flux_response_recv (flux_t h, JSON *respp, char **tagp, bool nb)
{
    zmsg_t *zmsg;
    int rc = -1;

    if (!(zmsg = flux_response_recvmsg (h, FLUX_MATCHTAG_NONE, nb)))
        goto done;
    if (flux_msg_get_errnum (zmsg, &errno) < 0 || errno != 0)
        goto done;
    if (flux_msg_decode (zmsg, tagp, respp) < 0)
        goto done;
    rc = 0;
done:
    if (zmsg)
        zmsg_destroy (&zmsg);
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

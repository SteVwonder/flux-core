/************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* simulator.c - sim interface
 *
 * This interface is primarily built so that the simulator can determine when
 * the system has become quiescent (assuming no further events from external
 * sources). Before responding to a quiescent request, the job-manager will
 * ensure that all of the relevant modules (e.g., sched, exec, and depend) are
 * also quiescent.
 */
#include <flux/core.h>

#include "simulator.h"

struct sim_ctx {
    flux_t *h;
    flux_msg_handler_t **handlers;
    flux_msg_t *sim_req;
    flux_future_t *sched_req;
};

void sim_ctx_destroy (struct sim_ctx *ctx)
{
    if (ctx) {
        int saved_errno = errno;;
        flux_msg_handler_delvec (ctx->handlers);
        if (ctx->sim_req)
            flux_msg_destroy(ctx->sim_req);
        free (ctx);
        errno = saved_errno;
    }
}

static void sched_quiescent_continuation(flux_future_t *f, void *arg)
{
    struct sim_ctx *ctx = arg;

    if (ctx->sim_req == NULL) {
        flux_log_error (ctx->h, "%s: sim quiescent request is NULL", __FUNCTION__);
        return;
    }
    if (ctx->sched_req != f) {
        flux_log_error (ctx->h, "%s: stored future does not match continuation future", __FUNCTION__);
        return;
    }

    const char *sched_payload = NULL;
    flux_rpc_get (f, &sched_payload);

    const char *sim_payload = NULL;
    flux_msg_get_string (ctx->sim_req, &sim_payload);
    flux_log (ctx->h, LOG_DEBUG, "receive quiescent from sched (%s), replying to sim with (%s)", sched_payload, sim_payload);
    flux_respond (ctx->h, ctx->sim_req, sim_payload);
    flux_future_destroy (f);
    flux_msg_destroy (ctx->sim_req);
    ctx->sched_req = NULL;
    ctx->sim_req = NULL;
}

void sim_sending_sched_request (struct sim_ctx *ctx)
{
    if (ctx->sim_req == NULL) {
        // either not in a simulation, or if we are, the simulator does not yet
        // care about tracking quiescence
        return;
    }
    if (ctx->sched_req != NULL) {
        // we are sending the scheduler more work/events before hearing back
        // from the previous quiescent request, destroy the future from that
        // previous request before sending a new request
        flux_future_destroy (ctx->sched_req);
    }

    flux_log (ctx->h, LOG_DEBUG, "sending quiescent req to scheduler");
    ctx->sched_req = flux_rpc (ctx->h, "sched.quiescent", NULL, 0, 0);
    if (ctx->sched_req == NULL)
        flux_respond_error(ctx->h, ctx->sim_req, errno, "job-manager: sim_sending_sched_request: flux_rpc failed");
    if (flux_future_then (ctx->sched_req, -1, sched_quiescent_continuation, ctx) < 0)
        flux_respond_error(ctx->h, ctx->sim_req, errno, "job-manager: sim_sending_sched_request: flux_future_then failed");
}


/* Handle a job-manager.quiescent request.  We'll first copy the request into
 * ctx for later response, and then kick off the process of verifying that all
 * relevant modules are quiesced.
 */
static void quiescent_cb (flux_t *h, flux_msg_handler_t *mh,
                          const flux_msg_t *msg, void *arg)
{
    struct sim_ctx* ctx = arg;
    ctx->sim_req = flux_msg_copy (msg, true);
    if (ctx->sim_req == NULL)
        flux_respond_error(h, msg, errno, "job-manager: quiescent_cb: flux_msg_copy failed");

    // Check if the scheduler is quiesced
    sim_sending_sched_request(ctx);
}

static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST, "job-manager.quiescent", quiescent_cb, 0},
    FLUX_MSGHANDLER_TABLE_END,
};

struct sim_ctx *sim_ctx_create (flux_t *h)
{
    struct sim_ctx *ctx;

    if (!(ctx = calloc (1, sizeof (*ctx))))
        return NULL;
    ctx->h = h;
    ctx->sim_req = NULL;
    if (flux_msg_handler_addvec (h, htab, ctx, &ctx->handlers) < 0)
        goto error;
    return ctx;
error:
    sim_ctx_destroy (ctx);
    return NULL;
}

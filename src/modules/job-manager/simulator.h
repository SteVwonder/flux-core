/************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _FLUX_JOB_MANAGER_SIMULATOR_H
#define _FLUX_JOB_MANAGER_SIMULATOR_H

#include <flux/core.h>

struct sim_ctx;

void sim_ctx_destroy (struct sim_ctx *ctx);
struct sim_ctx *sim_ctx_create (flux_t *h);

/* Call when sending a new rpc/work to the scheduler
 */
void sim_sending_sched_request (struct sim_ctx *ctx);

#endif /* ! _FLUX_JOB_MANAGER_SIMULATOR_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

===========================
FLUX_SHELL_SERVICE_REGISTER
===========================


NAME
====

flux_shell_service_register - Register a service handler for \`method\` in the shell.

SYNOPSIS
========

   ::

      #include <flux/shell.h>

..

   ::

      int flux_shell_service_register (flux_shell_t *shell,
                                       const char *method,
                                       flux_msg_handler_f cb,
                                       void *arg);

DESCRIPTION
===========

The job shell registers a unique service name with the flux broker on startup, and posts the topic string for this service in the context of the ``shell.init`` event. ``flux_shell_service_register()`` allows registration of a request handler ``cb`` for subtopic ``method`` on this service endpoint, allowing other job shells and/or flux commands to interact with arbitrary services within a job.

RETURN VALUE
============

Returns -1 on failure, 0 on success.

ERRORS
======

EINVAL

   ``shell``, ``method`` or ``cb`` is NULL.

AUTHOR
======

This page is maintained by the Flux community.

RESOURCES
=========

Github: <http://github.com/flux-framework>

SEE ALSO
========

``flux_msg_handler_create(3)``

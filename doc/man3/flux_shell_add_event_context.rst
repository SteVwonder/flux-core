============================
FLUX_SHELL_ADD_EVENT_CONTEXT
============================


NAME
====

flux_shell_add_event_context - Add context information for standard shell events

SYNOPSIS
========

   ::

      #include <flux/shell.h>
      #include <errno.h>

..

   ::

      int flux_shell_add_event_context (flux_shell_t *shell,
                                        const char *name,
                                        int flags,
                                        const char *fmt,
                                        ...);

DESCRIPTION
===========

Add extra context that will be emitted with shell standard event ``name`` using Jansson ``json_pack()`` style arguments. The ``flags`` parameter is currently unused.

RETURN VALUE
============

Returns 0 on success, -1 if ``shell``, ``name`` or ``fmt`` are NULL.

ERRORS
======

EINVAL

   ``shell``, ``name`` or ``fmt`` are NULL.

AUTHOR
======

This page is maintained by the Flux community.

RESOURCES
=========

Github: <http://github.com/flux-framework>

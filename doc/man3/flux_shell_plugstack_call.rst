=========================
FLUX_SHELL_PLUGSTACK_CALL
=========================


NAME
====

flux_shell_plugstack_call - Calls the function referenced by topic.

SYNOPSIS
========

   ::

      #include <flux/shell.h>

..

   ::

      int flux_shell_plugstack_call (flux_shell_t *shell,
                                     const char *topic,
                                     flux_plugin_arg_t *args);

DESCRIPTION
===========

The job shell implements a flexible plugin architecture which allows registration of one or more callback functions on arbitrary topic names. The stack of functions "listening" on a given topic string is called the "plugin stack". ``flux_shell_plugstack_call()`` exports the ability to call into the plugin stack so that plugins can invoke callbacks from other plugins.

RETURN VALUE
============

Returns 0 on success and -1 on failure, setting errno.

ERRORS:
=======

EINVAL

   ``shell`` or ``topic`` are NULL.

AUTHOR
======

This page is maintained by the Flux community.

RESOURCES
=========

Github: <http://github.com/flux-framework>

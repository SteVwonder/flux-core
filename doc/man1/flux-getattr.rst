============
FLUX-GETATTR
============


NAME
====

flux-getattr, flux-setattr, flux-lsattr - access broker attributes

SYNOPSIS
========

**flux** **getattr** *name*

**flux** **setattr** *name* *value*

**flux** **setattr** [*--expunge*] *name*

**flux** **lsattr** [*--values*]

DESCRIPTION
===========

Flux broker attributes are both a simple, general-purpose key-value store with scope limited to the local broker rank, and a method for the broker to export information needed by Flux comms modules and utilities.

flux-getattr(1) retrieves the value of an attribute.

flux-setattr(1) assigns a new value to an attribute, or optionally removes an attribute.

flux-lsattr(1) lists attribute names, optionally with their values.

RESOURCES
=========

Github: <http://github.com/flux-framework>

SEE ALSO
========

flux_attr_get(3), flux-broker-attributes(7)

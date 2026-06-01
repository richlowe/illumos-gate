#! /usr/bin/ksh
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

# Copyright 2025 Richard Lowe

# Simple database operations which should work
# A very basic initial round of tests for the sqlite3 upgrade.

prefix=idmap_test.$$

must() {
	"$@" >/dev/null
	if (( $? != 0 )); then
		print -u2 "FAIL: $@";
	fi
}

must_not() {
	"$@" >/dev/null
	if (( $? == 0 )); then
		print -u2 "FAIL: $@";
	fi
}

must idmap add unixuser:${prefix}user winuser:${prefix}user
must idmap add unixgroup:${prefix}group wingroup:${prefix}group
must idmap add "unixuser:${prefix}a*" winuser:"${prefix}a*@example.com"
must idmap add "unixgroup:${prefix}g*" wingroup:"${prefix}g*@example.com"
must idmap add unixuser:${prefix}notgroup wingroup:${prefix}agroupthough # XXX
must idmap add unixgroup:${prefix}ugroup winuser:${prefix}wuser		 # XXX


# Less specific rules
must idmap add "unixuser:${prefix}*" "winuser:${prefix}*@example.com"
must idmap add "unixgroup:${prefix}*" "wingroup:${prefix}*@example.com"

# Straight Duplicates
must_not idmap add unixuser:${prefix}user winuser:${prefix}user
must_not idmap add unixgroup:${prefix}group wingroup:${prefix}group
must_not idmap add "unixuser:${prefix}a*" winuser:"${prefix}a*@example.com"
must_not idmap add "unixgroup:${prefix}g*" wingroup:"${prefix}g*@example.com"
must_not idmap add unixuser:${prefix}notgroup wingroup:${prefix}agroupthough # XXX
must_not idmap add unixgroup:${prefix}ugroup winuser:${prefix}wuser		 # XXX

# Case conflicts
must_not idmap add unixuser:${prefix}otheruser winuser:${prefix}USER
must_not idmap add unixgroup:${prefix}othergroup wingroup:${prefix}GROUP

# Conflicts
must_not idmap add unixuser:${prefix}user winuser:${prefix}someoneelse
must_not idmap add unixgroup:${prefix}group wingroup:${prefix}anothergroup
must_not idmap add "unixuser:${prefix}a*" winuser:"${prefix}a*@mystery.foo"
must_not idmap add "unixgroup:${prefix}g*" wingroup:"${prefix}g*@mystery.com"

# Clean up unfortunately manually
must idmap remove unixuser:${prefix}user
must idmap remove unixgroup:${prefix}group
must idmap remove "unixuser:${prefix}a*"
must idmap remove "unixgroup:${prefix}g*"
must idmap remove unixuser:${prefix}notgroup
must idmap remove unixgroup:${prefix}ugroup
must idmap remove "unixuser:${prefix}*"
must idmap remove "unixgroup:${prefix}*"

-- CDDL HEADER START
--
-- The contents of this file are subject to the terms of the
-- Common Development and Distribution License (the "License").
-- You may not use this file except in compliance with the License.
--
-- You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
-- or http://www.opensolaris.org/os/licensing.
-- See the License for the specific language governing permissions
-- and limitations under the License.
--
-- When distributing Covered Code, include this CDDL HEADER in each
-- file and include the License file at usr/src/OPENSOLARIS.LICENSE.
-- If applicable, add the following below this CDDL HEADER, with the
-- fields enclosed by brackets "[]" replaced with your own identifying
-- information: Portions Copyright [yyyy] [name of copyright owner]
--
-- CDDL HEADER END

-- Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
-- Use is subject to license terms.

-- This versions the schema.  This is version 1.
PRAGMA user_version = 1;

CREATE TABLE idmap_cache(
	sidprefix TEXT NOT NULL,
	rid INTEGER NOT NULL,
	windomain TEXT,
	canon_winname TEXT,
	winname TEXT,
	pid INTEGER NOT NULL,
	unixname TEXT,
	is_user INTEGER,
	is_wuser INTEGER,
	w2u INTEGER NOT NULL,
	u2w INTEGER NOT NULL,
	map_type INTEGER,
	map_dn TEXT,
	map_attr TEXT,
	map_value TEXT,
	map_windomain TEXT,
	map_winname TEXT,
	map_unixname TEXT,
	map_is_nt4 INTEGER,
	expiration INTEGER NOT NULL);

CREATE UNIQUE INDEX idmap_cache_sid_w2u ON idmap_cache(sidprefix, rid, is_user,
    w2u);

CREATE UNIQUE INDEX idmap_cache_pid_u2w ON idmap_cache(pid, is_user, u2w);

CREATE TRIGGER idmap_cache_tolower_name_insert AFTER INSERT ON idmap_cache
BEGIN
	UPDATE idmap_cache SET winname = lower(canon_winname)
		WHERE rowid = new.rowid;
END;

CREATE TRIGGER idmap_cache_tolower_name_update AFTER UPDATE ON idmap_cache
BEGIN
	UPDATE idmap_cache SET winname = lower(canon_winname)
		WHERE rowid = new.rowid;
END;

CREATE TABLE name_cache (
	sidprefix TEXT,
	rid INTEGER,
	name TEXT,
	canon_name TEXT,
	domain TEXT,
	type INTEGER,
	expiration INTEGER
);

CREATE TRIGGER name_cache_tolower_name_insert  AFTER INSERT ON name_cache
BEGIN
	UPDATE name_cache SET name = lower(canon_name)
		WHERE rowid = new.rowid;
END;

CREATE TRIGGER name_cache_tolower_name_update AFTER UPDATE ON name_cache
BEGIN
	UPDATE name_cache SET name = lower(canon_name)
		WHERE rowid = new.rowid;
END;

CREATE UNIQUE INDEX name_cache_sid ON name_cache(sidprefix, rid);

CREATE UNIQUE INDEX name_cache_name ON name_cache(name, domain);

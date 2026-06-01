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

CREATE TABLE namerules(
	is_user INTEGER NOT NULL,
	is_wuser INTEGER NOT NULL,
	windomain TEXT,
	winname_display TEXT NOT NULL,
	winname TEXT,
	is_nt4 INTEGER NOT NULL,
	unixname NOT NULL,
	w2u_order INTEGER,
	u2w_order INTEGER);

CREATE UNIQUE INDEX namerules_w2u ON namerules(winname, windomain,
    is_user, is_wuser, w2u_order);

CREATE UNIQUE INDEX namerules_u2w ON namerules(unixname, is_user, u2w_order);

CREATE TRIGGER namerules_tolower_name_insert AFTER INSERT ON namerules
BEGIN
	UPDATE namerules SET winname = lower(winname_display)
	    WHERE rowid = new.rowid;
END;

CREATE TRIGGER namerules_tolower_name_update AFTER UPDATE ON namerules
BEGIN
	UPDATE namerules SET winname = lower(winname_display)
	    WHERE rowid = new.rowid;
END;

CREATE TRIGGER namerules_unique_insert BEFORE INSERT ON namerules
BEGIN
	SELECT CASE (SELECT count(*) FROM namerules AS n
		WHERE n.unixname = NEW.unixname AND
		n.is_user = NEW.is_user AND
		(n.winname != lower(NEW.winname_display) OR
		n.windomain != NEW.windomain ) AND
		n.u2w_order = NEW.u2w_order AND
		n.is_wuser != NEW.is_wuser) > 0
	WHEN 1 THEN
		raise(ROLLBACK, 'Conflicting w2u namerules')
	END;
END;

CREATE TRIGGER namerules_unique_update BEFORE UPDATE ON namerules
BEGIN
	SELECT CASE (SELECT count(*) FROM namerules AS n
		WHERE n.unixname = NEW.unixname AND
		n.is_user = NEW.is_user AND
		(n.winname != lower(NEW.winname_display) OR
		n.windomain != NEW.windomain ) AND
		n.u2w_order = NEW.u2w_order AND
		n.is_wuser != NEW.is_wuser) > 0
	WHEN 1 THEN
		raise(ROLLBACK, 'Conflicting w2u namerules')
	END;
END;

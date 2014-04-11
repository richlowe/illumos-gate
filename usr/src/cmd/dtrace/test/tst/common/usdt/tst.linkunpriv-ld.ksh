#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIR=/var/tmp/dtest.$$

mkdir $DIR
cd $DIR

ppriv -s A=basic $$

cat > test.c <<EOF
#include <sys/sdt.h>
#include "prov.h"

int
main(int argc, char **argv)
{
	TEST_PROV_ZERO();
	TEST_PROV_ONE(1);
	TEST_PROV_TWO(2, 3);
	TEST_PROV_THREE(4, 5, 7);
	TEST_PROV_FOUR(7, 8, 9, 10);
	TEST_PROV_FIVE(11, 12, 13, 14, 15);
}
EOF

cat > prov.d <<EOF
provider test_prov {
	probe zero();
	probe one(uintptr_t);
	probe two(uintptr_t, uintptr_t);
	probe three(uintptr_t, uintptr_t, uintptr_t);
	probe four(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
	probe five(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
};
EOF

$dtrace -h -s prov.d

gcc -m32 -c test.c
if [ $? -ne 0 ]; then
	print -u2 "failed to compile test.c"
	exit 1
fi

gcc -m32 -o test test.o
if [ $? -ne 0 ]; then
	print -u2 "failed to link final executable"
	exit 1
fi

cd /
/usr/bin/rm -rf $DIR

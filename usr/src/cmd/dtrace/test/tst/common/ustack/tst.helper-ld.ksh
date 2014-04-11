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

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIR=/var/tmp/dtest.$$

mkdir $DIR
cd $DIR

cat > helper.d <<EOF
dtrace:helper:ustack:
{
	"@it's working"
}
EOF

$dtrace -h -s helper.d
if [ $? -ne 0 ]; then
	print -u2 "failed to generate header file"
	exit 1
fi

cat > test.c <<EOF
#include <sys/types.h>
#include "helper.h"

int
baz()
{
	return (getpid() + 8);
}

int
main(int argc, char **argv)
{
	while (1) 
		baz();
}
EOF

gcc -m32 -o test test.c
if [ $? -ne 0 ]; then
	print -u2 "failed to compile test.c"
	exit 1
fi

script()
{
	$dtrace -c ./test -qs /dev/stdin <<EOF
	pid\$target::baz:entry
	{
		ustack(1, 1024);
		exit(0);
	}
EOF
}

script
status=$?

exit $status

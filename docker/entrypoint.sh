#!/bin/bash

# Check if the capabilities associated to the executable are available from the current Bounding set.
# The Bounding set is used because the Permitted/Inheritable/Effective sets are cleared when we switch to an unprivileged user in the image.
#
REQUIRED_CAP=$(/sbin/getcap /usr/bin/hwpc-sensor |sed -e 's/.*\(cap_.*\)[+=].*$/\1/')
REQUIRED_CAP_AVAILABLE=$([ -n "${REQUIRED_CAP}" ] && /sbin/capsh --print |grep "Bounding set =" |grep -iqv "${REQUIRED_CAP}" ; echo $?)
if [ "${REQUIRED_CAP_AVAILABLE}" -eq 0 ]; then
    echo >&2 "ERROR: This program requires the '${REQUIRED_CAP^^}' capability to work."
    exit 1
fi

exec /usr/bin/hwpc-sensor "$@"

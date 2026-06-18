#!/bin/bash

# Check that the required capability is present in the current bounding set.
# The bounding set is preserved across the switch to the unprivileged user, whereas the effective/permitted/inheritable sets are not.
if ! /sbin/capsh --has-b=cap_perfmon >/dev/null 2>&1 && ! /sbin/capsh --has-b=cap_sys_admin >/dev/null 2>&1; then
    echo >&2 "ERROR: Missing required capability: CAP_PERFMON (or CAP_SYS_ADMIN on older kernels)."
    echo >&2 "Hint: Docker/Podman: --cap-add=PERFMON; Kubernetes: securityContext.capabilities.add."
    exit 1
fi

exec /usr/bin/hwpc-sensor "$@"

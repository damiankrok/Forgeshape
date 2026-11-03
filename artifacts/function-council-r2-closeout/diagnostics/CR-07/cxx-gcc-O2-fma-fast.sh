#!/bin/sh
exec g++ "$@" -O2 -mfma -ffp-contract=fast

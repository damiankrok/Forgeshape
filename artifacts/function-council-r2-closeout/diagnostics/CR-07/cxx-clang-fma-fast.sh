#!/bin/sh
exec clang++ -mfma -ffp-contract=fast "$@"

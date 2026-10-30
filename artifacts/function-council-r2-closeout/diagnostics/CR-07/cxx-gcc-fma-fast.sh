#!/bin/sh
exec g++ -mfma -ffp-contract=fast "$@"

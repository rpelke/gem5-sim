#!/bin/bash
set -euo pipefail

# Get directory of script
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
    DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="${DIR}/$SOURCE"
done
MAIN_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"

cd "${MAIN_DIR}/gem5"

# Print python3 path
echo "Using python3 at: $(which python3)"

# Check if scons is installed
if ! command -v scons >/dev/null 2>&1; then
    echo "Error: 'scons' not found. Install it with: sudo apt install scons"
    exit 1
fi

EXTRAS_DIR="${MAIN_DIR}/accelerator"

# Build ARM target
scons build/ARM/gem5.opt EXTRAS="${EXTRAS_DIR}" -j"$(nproc)"
if [ ! -f "${MAIN_DIR}/gem5/build/ARM/gem5.opt" ]; then
    echo "Gem5 ARM build was not successful."
    exit 1
fi

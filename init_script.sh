#!/bin/bash

 git submodule update --init --recursive

 cp dpdk-app-patch.patch dpdk/

 cd dpdk/ || { echo "DPDK submodule not found"; exit 1; }

 git apply --whitespace=nowarn dpdk-app-patch.patch

 meson -Dexamples=all build

ninja -C build

sleep 1

cd build/examples/

ln -sf "$(pwd)/dpdk-test_app_parse" ../../../dpdk-test_app_parse

echo "DPDK and custom application build completed successfully"
echo "You can now run the application."



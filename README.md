# Purpose
zig-perftest: application: zig build of https://github.com/linux-rdma/perftest raw_ethernet_bw

# Setting Up Machine
On a fresh bare metal Ubuntu 22.04LTS box with Infiniband (IB) compatible NICs run `scripts/setup_build_env`

# Build Program
In top-level of this respository run `zig build`

# Configure
Edit `scripts/run` to suite. Usually one sends and receives packets over the same NIC on same machine

# Running Program
* Run `scripts/setup_run_env` after reboot.
* On server machine: `scripts/run server`
* On client machine: `scripts/run client`

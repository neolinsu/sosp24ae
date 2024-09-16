# Overview of Vessel-AE
Hi!
In this AE repository, we provide a Vessel prototype (Vessel-AE) to evaluate our uProcess design.
We will open-source Vessel ready for release soon.

Vessel-AE consists of three components, `vessel`, `veiokerneld`, `vessel-cli`.

* `vessel` inits the basic runtime environment, e.g., loading the vessel runtime, which gets a kernel process prepared to load an application within it's binding uProcess.
* `veiokerneld` is the global scheduler that pulls running status of each uProcess and schedules via Uintr
* `vessel-cli` send the running cmd to `vessel`, indicating the path of application with `argc`, `argv`, `envs` and other args.

The following instructions aims at running a bench in `apps/switch-vessel` in Fedora 35 with kernel `5.16.15`.
You may leave an issue for other setups.

# How to run Vessel?
1. Download the following projects' source code into `third_party`.
   1. `dpdk 22.03.0`: `third_party/dpdk`.
   2. `pcm` [e0b4c6b435](https://github.com/intel/pcm/commit/e0b4c6b435cc04b08d9fba472ebc669e36db746c): `third_party/pcm`.
   3. `rdma-core v32.0`: `third_party/rdma-core`.
2. Run `scripts/setup.sh` to build all the third parties.
3. Install [Linux-uintr](https://github.com/neolinsu/linux-uintr)
   ```bash
   cp /boot/config-5.15.16-200.fc35.x86_64 .config
   make oldconfig
   make localmodconfig
   make && make install && make modules_install
   ```
4. Compile [glibc-vessel](https://github.com/neolinsu/glibc-vessel)
   1. Choose a glibc version that according to the local env (`ldd --version`). Note that even with the same version code, the implementation can be different. So Vessel's modification should be patched to the exact glibc package source code, if the loader in [glibc-vessel](https://github.com/neolinsu/glibc-vessel) can not function. Feel free to contact, if you need a more glibc version.
   2. `mkdir build&& cd build && ../configure --prefix=${pwd}/dist`
   3. Copy the loader bin `ld-linux-x86-64.so.2` to vessel path `${VESSEL_ROOTPATH}//third_party_libs/ld-linux-x86-64-vessel.so.2` 
5. Compile Vessel.
The following commands build and install Vessel into `dist/`, and `LD_LIBRARY_PATH` and `PATH` are modified to contain Vessel environments.

   ```bash
   source ./scripts/env.rc
   make -j && make install
   ```

# switch-vessel
To run `apps/switch-vessel`, the `scripts/vessel_switch.py` in this path will do the compiling from source and run all the benchmarks.

```bash
cd apps/switch-vessel
python scripts/vessel_switch.py
```


# Fast Core Scheduling with Userspace Process Abstraction

## Evaluation Environment

We provide the environment consisting of one server and four clients.
Please check [hotcrp](https://sosp24ae.hotcrp.com/) for the access method.

* The server node is equipped with two 32-core Intel 4th Gen Xeon Gold 6430 processors and 256GB of RAM running Fedora 35 with [kernel 5.16.15](./linux-uintr) (patched with Intel's UIPI support).
* Each client node is equipped with two 18-physical-core Intel(R) Xeon(R) Gold 5220 CPU processors and 128GB of RAM. 
* These server and client nodes are connected with the 100Gbps ConnectX-6 Mellanox network.

These nodes have installed dependencies of Vessel and the compared systems.


## Note
1. Before evaluating Vessel, please run `w` to check if anyone else is using it, to avoid resource contention. And please close ssh connection when not conducting evalation.
2. The evaluation can be still running, when ssh connection is close. The evaluation scripts will keeps waiting, util the on-going evaluation finished. 
3. If you have any question, you can contact with me via [linjz20@mails.tsinghua.edu.cn](mailto:linjz20@mails.tsinghua.edu.cn)

## Directory Organization
1. `glibc-vessel`: the ld loader for Vessel.
2. `linux-uintr`: linux kernel patched with Intel's UIPI support.
3. `vessel`: vessel implementation. 
   * `third_party`
     * `dpdk`: the patched dpdk for userspace datapath.
     * `rdma-core`: the patched rdma for userspace datapath.
     * `jemalloc-vessel`: the modified jemalloc for memory management.
     * `pcm`: pcm for cpu performance register information.
   * `apps`: benchmark apps for linkpack, memory stressing, context switch, cache conflict, and etc.
   * `src`: 
     * `core/uipi.c`: UIPI handler
     * `core/cluster.c`: uProcess
     * `core/mem.c`: memory management and MPK.
4. `caladan-ae`: we use Caladan from [caladan-aritfact](https://github.com/joshuafried/caladan-artifact).

## How to start?

On the monitor node, run:

```bash
cd ./sosp24-ae/scripts
bash all.sh
```

The result of each figure or table in the evaluation section will be printed to the `stdout`,
including:
   * Figure 9~12
   * Table 1
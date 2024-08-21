## Overview

This repository contains a collection of TCP and UDP RPC servers for use with Linux, [IX](https://github.com/ix-project/ix), and [Zygos](https://github.com/ix-project/zygos). All the servers implement the binary memcached protocol, thus they can be used in conjunction with memcached benchmark tools, such as [mutilate](https://github.com/ix-project/mutilate).

## Servers

Each server performs a different operation for every GET request:

* `spin`: spins the CPU for N seconds, where N is a random variable that follows a specified in the command line distribution.
* `silotpcc`: executes a random TPC-C transaction on the [Silo](https://github.com/ix-project/silo) in-memory database.

#! /bin/bash
root_path=$(cd $(dirname $0)/../; pwd)
echo Path to vessel: ${root_path}

cd $root_path/third_party/pcm && \
rm -rf build

cd $root_path/third_party/rdma-core && \
rm -rf build

cd $root_path/third_party/dpdk && \
rm -rf build
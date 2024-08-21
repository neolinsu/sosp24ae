#! /bin/bash
root_path=$(cd $(dirname $0)/../; pwd)
echo Path to vessel: ${root_path}

# Third parties

mkdir -p $root_path/third_party_libs

echo BUILDING jemalloc-vessel
cd $root_path/third_party/jemalloc-vessel && \
mkdir build && \
cd build && \
../configure --with-malloc-conf="dirty_decay_ms:-1,muzzy_decay_ms:-1" \
        --disable-initial-exec-tls \
        --prefix=`pwd`/dist &&\
make clean && make -j && make install && \
cp dist/lib/libjemalloc.so.2 $root_path/third_party_libs/ && \
echo jemalloc-vessel done.

## PCM
echo BUILDING PCM
cd $root_path/third_party/pcm && \
git submodule update --init --recursive && \
mkdir -p build && \
cd build && \
cmake .. && \
cmake --build . &&\
echo PCM done.

## 
echo BUILDING rdma-core
cd $root_path/third_party/rdma-core && \
git checkout . && \
git apply $root_path/patches/rdma-core.patch
if ! EXTRA_CMAKE_FLAGS="-DENABLE_STATIC=1 -DCMAKE_C_FLAGS=-fPIE" ./build.sh; then
  echo "Building rdma-core failed"
  echo "If you see \"Does not match the generator used previously\" try running \"make submodules-clean\" first"
  exit 1
fi

echo rdma-core done.

## DPDK
echo BUILDING DPDK
disable_driver='crypto/*,net/bnxt'

if lspci | grep -q 'ConnectX-[4,5,6]'; then
  # build against local rdma-core library
  export EXTRA_CFLAGS=-I$root_path/third_party/rdma-core/build/include
  export EXTRA_LDFLAGS=-L$root_path/third_party/rdma-core/build/lib
  export PKG_CONFIG_PATH=$root_path/third_party/rdma-core/build/lib/pkgconfig
elif lspci | grep -q 'ConnectX-3'; then
  rm -f dpdk/drivers/net/mlx4/mlx4_custom.h
  patch -p1 -N -d dpdk/ < build/mlx4_22_03.patch
  disable_driver="${disable_driver},common/mlx5,net/mlx5"
fi

cd $root_path/third_party/dpdk
meson build
meson configure -Ddisable_drivers=$disable_driver build
meson configure -Dprefix=$PWD/build -Dmax_lcores=256 build
ninja -C build
ninja -C build install
echo dpdk done.

exit 0

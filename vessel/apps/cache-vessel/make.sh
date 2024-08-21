echo "Compiling... with $cflags"
make clean
make cache2 MYCFLAGS="-DPID=2 $cflags"
make cache MYCFLAGS="-DPID=1 $cflags"

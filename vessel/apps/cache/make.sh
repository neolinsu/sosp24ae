echo "Compiling... with $cflags"
gcc cache2.c -O3 -ffast-math -march=native -o ./cache2 -DPID=2 $cflags
gcc cache.c -O3 -ffast-math -march=native -o ./cache -DPID=1 $cflags
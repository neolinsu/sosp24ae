
# Client Infos
client_infos = {
    143: {
        "nicpci": "000:3b:00.0",
    },
    144: {
        "nicpci": "000:3b:00.0",
    },
    145: {
        "nicpci": "000:3b:00.0",
    },
    147: {
        "nicpci": "000:3b:00.0",
    },
}


# Figure 9
figure_9_vessel_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/vessel",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "ias nobw",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
    "vessel_cpus": "2-17,66-81",
}

figure_9_caladan_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-ae",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "ias nobw",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
}

figure_9_drl_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-dr",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "simple range_policy interval 5",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
}

figure_9_drh_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-dr",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "simple range_policy interval 5",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
}

figure_9_caladan_client_config_template = { # Caladan Client
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-ae",
    "remote_host": "PLACEHOLDER",
    "password": "sosp24ae!",
    "sched_config": "ias nobw",
    "nicpci": "PLACEHOLDER",
    "iokernel_cpus": "1-17,38-53",
}

figure_9_app_path = {
    "vessel": {
        "silo": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/silo/silo",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/vessel_lc.config",
            "cmd": "32 5005 3221225472"
        },
        "memcached": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/memcached/memcached",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/vessel_lc.config",
            "cmd": "-c 32768 -m 32000 -b 32768 -o hashpower=25,no_hashexpand,lru_crawler,lru_maintainer,idle_timeout=0,slab_reassign"
        },
        "linpack": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/membench-vessel/calbench",
            "config": "/home/sosp24ae/sosp24-ae/vessel/apps/membench-vessel/be.config",
            "cmd": "-t32 -c1"
        },
        "gather": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/membench-vessel/calgather",
            "cmd": ""
        }
    },
    "caladan": {
        "silo": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/apps/silo/silo",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/caladan_lc.config",
            "cmd": "32 5005 3221225472"
        },
        "memcached": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/apps/memcached/memcached",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/caladan_lc.config",
            "cmd": "-c 32768 -m 32000 -b 32768 -o hashpower=25,no_hashexpand,lru_crawler,lru_maintainer,idle_timeout=0,slab_reassign"
        },
        "linpack": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/membench-caladan/calbench",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/membench-caladan/be.config",
            "cmd": "-t32 -c1"
        },
        "gather": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/membench-caladan/calgather",
            "cmd": ""
        }
    }
}
figure_9_syn = {
    "path": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/synthetic/target/release/synthetic",
    "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/syn.config",
    "cmd": "192.168.2.199:5092 --output=buckets --mode runtime-client --threads 200 --runtime 20 --mean=842 --distribution=exponential --transport tcp --start_mpps 0.0 --nvalues=32000000"
}

# Figure 10
figure_10_vessel_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/vessel",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "ias nobw",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
    "vessel_cpus": "2-17,66-81",
}

figure_10_drl_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-dr",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "simple range_policy interval 5",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
}

figure_10_caladan_client_config_template = { # Caladan Client
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-ae",
    "remote_host": "PLACEHOLDER",
    "password": "sosp24ae!",
    "sched_config": "ias nobw",
    "nicpci": "PLACEHOLDER",
    "iokernel_cpus": "1-17,38-53",
}

figure_10_app_path = {
    "vessel": {
        "memcached": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/memcached/memcached",
            "config": "",
            "cmd": "-c 32768 -m 32000 -b 32768 -o hashpower=25,no_hashexpand,lru_crawler,lru_maintainer,idle_timeout=0,slab_reassign"
        }
    },
    "caladan": {
        "memcached": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/apps/memcached/memcached",
            "config": "",
            "cmd": "-c 32768 -m 32000 -b 32768 -o hashpower=25,no_hashexpand,lru_crawler,lru_maintainer,idle_timeout=0,slab_reassign"
        }
    }
}
figure_10_syn = {
    "path": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/synthetic/target/release/synthetic",
    "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/syn.config",
    "cmd": "--output=buckets --mode runtime-client --threads 200 --runtime 20 --mean=842 --distribution=exponential --transport tcp --start_mpps 0.0 --nvalues=32000000"
}

# Figure 11

figure_11_linux_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/vessel",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
}

# Figure 12

figure_12_vessel_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/vessel",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "ias",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
    "vessel_cpus": "2-17,66-81",
}

figure_12_caladan_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-ae",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "ias",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-17,65-81",
}
figure_12_caladan_client_config_template = { # Caladan Client
    "root_path": "/home/sosp24ae/sosp24-ae/caladan-ae",
    "remote_host": "PLACEHOLDER",
    "password": "sosp24ae!",
    "sched_config": "ias nobw",
    "nicpci": "PLACEHOLDER",
    "iokernel_cpus": "1-17,38-53",
}
figure_12_app_path = {
    "vessel": {
        "memcached": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/memcached/memcached",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/vessel_lc.config",
            "cmd": "-c 32768 -m 32000 -b 32768 -o hashpower=25,no_hashexpand,lru_crawler,lru_maintainer,idle_timeout=0,slab_reassign"
        },
        "membench": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/membench-vessel/membench",
            "config": "/home/sosp24ae/sosp24-ae/vessel/apps/membench-vessel/be.config",
            "cmd": "-t32 -c1"
        },
        "gather": {
            "path": "/home/sosp24ae/sosp24-ae/vessel/apps/membench-vessel/memgather",
            "cmd": ""
        }
    },
    "caladan": {
        "memcached": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/apps/memcached/memcached",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/caladan_lc.config",
            "cmd": "-c 32768 -m 32000 -b 32768 -o hashpower=25,no_hashexpand,lru_crawler,lru_maintainer,idle_timeout=0,slab_reassign"
        },
        "membench": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/membench-caladan/membench",
            "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/membench-caladan/be.config",
            "cmd": "-t32 -c1"
        },
        "gather": {
            "path": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/membench-caladan/memgather",
            "cmd": ""
        }
    }
}
figure_12_syn = {
    "path": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/synthetic/target/release/synthetic",
    "config": "/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/syn.config",
    "cmd": "--output=buckets --mode runtime-client --threads 200 --runtime 20 --mean=842 --distribution=exponential --transport tcp --start_mpps 0.0 --nvalues=32000000"
}

# Table 1
table_1_vessel_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/vessel",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
    "sched_config": "ias nobw",
    "nicpci": "0000:a8:00.0",
    "iokernel_cpus": "1-2,65-66",
    "vessel_cpus": "2,66",
}

table_1_linux_server_config = {
    "root_path": "/home/sosp24ae/sosp24-ae/vessel",
    "remote_host": "sosp24ae@10.0.2.199",
    "password": "sosp24ae!",
}

table_1_vessel_switch_template = {}
table_1_linux_switch_template = {}

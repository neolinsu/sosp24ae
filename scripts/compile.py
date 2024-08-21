import sys
from ctl_plane.terminal.bash import Bash

user = {
    "name": "sosp24ae",
    "password": "sosp24ae!",
}

clients = [143, 144, 145, 147]
servers = [199]


def build_client(args):
    bash = Bash(
        password=user["password"],
        remotehost=f"{user['name']}@10.0.2.{args['client']}",
        need_su=False,
    )
    bash.process.logfile_read = sys.stdout.buffer
    bash.run_cmd("cd ~/sosp24-ae/caladan-ae")
    bash.run_cmd("pkill python")
    bash.run_cmd("pkill iokernel")
    bash.run_cmd("pkill memcached")
    bash.run_cmd("pkill silo")
    bash.run_cmd("bash build_client.sh", expect_str="DONE_BUILD", timeout=1200)
    bash.terminate(force=True)


def build_all_clients():
    for client in clients:
        args = {"client": client}
        build_client(args)


def build_server(args):
    bash = Bash(
        password=user["password"],
        remotehost=f"{user['name']}@10.0.2.{args['server']}",
        need_su=False,
    )
    bash.process.logfile_read = sys.stdout.buffer
    bash.run_cmd("cd ~/sosp24-ae/vessel")
    bash.run_cmd("pkill python")
    bash.run_cmd("pkill vessel")
    bash.run_cmd("pkill veiokerneld")
    bash.run_cmd("bash build_all.sh", expect_str="DONE_BUILD", timeout=1200)
    bash.terminate(force=True)


def compile_all():
    build_all_clients()
    build_server({"server": servers[0]})


if __name__ == "__main__":
    compile_all()

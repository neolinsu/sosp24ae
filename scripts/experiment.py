import subprocess

import os
from fcntl import flock, LOCK_EX, LOCK_NB
from compile import compile_all

os.system("touch ./experiment_lock")
fd = open("./experiment_lock", "w")
flock(fd.fileno(), LOCK_EX | LOCK_NB)


def print_divider():
    print("\n--------------------------------------------------\n")


def run_experiment(name):
    subprocess.run(
        f"python ./experiments/{name}.py",
        shell=True,
        text=True,
    )


def run_all_experiments():
    print("# Evaluations")
    print("## Figure 9")
    run_experiment("figure_9")
    print_divider()

    print("## Figure 10")
    run_experiment("figure_10")
    print_divider()

    print("## Figure 11")
    run_experiment("figure_11")
    print_divider()

    print("## Figure 12")
    run_experiment("figure_12")
    print_divider()

    print("## Table 1")
    run_experiment("table_1")
    print_divider()


if __name__ == "__main__":
    compile_all()
    run_all_experiments()

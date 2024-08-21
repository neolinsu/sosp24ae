import os
import logging
import subprocess


EXP_BASE = "./"
SHELL = "/bin/bash"

LOGGER = logging.getLogger('experiment')
logging.basicConfig(format='%(asctime)s: %(message)s', level=logging.INFO)


def _runcmd(cmdstr, outp, suppress=False, **kwargs):
    kwargs['executable'] = kwargs.get("executable", SHELL)
    kwargs['cwd'] = kwargs.get('cwd', EXP_BASE)
    pfn = LOGGER.debug if True or suppress else LOGGER.info
    if outp:
        pfn("running {%s}: " % cmdstr)
        res = subprocess.check_output(cmdstr, shell=True, **kwargs)
#        pfn("%s\n" % res.strip())
        return res
    else:
        p = subprocess.Popen(cmdstr, shell=True,
                             stdin=subprocess.PIPE, **kwargs)
        LOGGER.info("[%04d]: launched {%s}" % (p.pid, cmdstr))
        return p

def launch(*args, **kwargs):
    assert 'outp' not in kwargs
    assert len(args) == 1
    return _runcmd(args[0], False, **kwargs)

def runcmd(*args, **kwargs):
    assert 'outp' not in kwargs
    assert len(args) == 1
    return _runcmd(args[0], True, **kwargs)

def mask_to_list(mask):
    i = 0
    l = []
    while mask:
        if mask & 1: l.append(i)
        mask >>= 1
        i += 1
    return l

def list_to_mask(l):
    i = 0
    for w in l:
        i |= 1 << w
    return i

def bind_core(core):
    control_core_mask=hex(list_to_mask([core]))[2:]
    proc = os.listdir('/proc/')
    for p in proc:
        if not os.access("/proc/{}/task".format(p), os.F_OK):
            continue
        if not p.isdigit():
            continue

        cur_mask = runcmd("taskset -p {}".format(p), suppress=True
                            ).decode().split(": ")[-1].strip()
        cur_mask = set(mask_to_list(int(cur_mask, 16)))

        # taskset [options] -p [mask] pid
        runcmd("sudo taskset -p {} {} > /dev/null 2>&1 || true".format(control_core_mask, p), suppress=True)
    if(len(control_core_mask)==10):
        control_core_mask=control_core_mask[0:2]+","+control_core_mask[2:]
    if(len(control_core_mask)==9):
        control_core_mask=control_core_mask[0:1]+","+control_core_mask[1:]
    # migrate irqs
    irqs = os.listdir('/proc/irq/')
    for i in irqs:
        if i == "0":
            continue
        runcmd("echo {} | sudo tee /proc/irq/{}/smp_affinity > /dev/null 2>&1 || true".format(control_core_mask, i), suppress=True)

    runcmd("echo {} | sudo tee /sys/bus/workqueue/devices/writeback/cpumask > /dev/null 2>&1".format(control_core_mask), suppress=True)


bind_core(0)
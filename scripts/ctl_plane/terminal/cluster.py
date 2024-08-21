import pexpect
import json
from .bash import Bash

class VCluster:
    @classmethod
    def read_res(cls, b):
        print(b)
        return json.loads(b)

    def __init__(self,
                 vessel_name:str,
                 root_path:str='./',
                 password:str=None,
                 remotehost:str=None,
                 iokernel_cpus:str=None,
                 vessel_cpus:str=None,
                 sched_config:str=None,
                 nicpci:str=None):
        self.veiokerneld_bash = Bash(
            f'source {root_path}/scripts/env.rc',
            '.*SOURCE ENV DONE.',
            password, remotehost)
        if not self.veiokerneld_bash.is_alive():
            raise RuntimeError(f"Fail to run bash with '{root_path}' and '{password}'")
        self.veiokerneld_bash.run_cmd(
            f'export VESSEL_CXL_NODE_ID=0',
            None)
        try:
            self.veiokerneld_bash.run_cmd(
                "killall -9 veiokerneld", None
            )
            self.veiokerneld_bash.run_cmd(
                "killall -9 vessel", None
            )
            self.veiokerneld_bash.run_cmd(
                f'VESSEL_NAME={vessel_name} veiokerneld {sched_config} {iokernel_cpus} nicpci {nicpci} 2>&1',
                'running dataplane'
            )
        except RuntimeError:
            self.veiokerneld_bash.process.interact()
            exit()

        self.vessel_bash = Bash(
            f'source {root_path}/scripts/env.rc',
            '.*SOURCE ENV DONE.',
            password, remotehost)
        if not self.vessel_bash.is_alive():
            raise RuntimeError(f"Fail to run bash with '{root_path}' and '{password}'")
        self.vessel_bash.run_cmd(
            f'export VESSEL_FORCE_INIT_VESSEL_META=1',
            None)
        self.vessel_bash.run_cmd(
            f'export VESSEL_FORCE_INIT_MEMORY_META=1',
            None)
        self.vessel_bash.run_cmd(
            f'VESSEL_NAME={vessel_name} vessel {vessel_cpus} 2>&1',
            '------------')

        self.vessel_name = vessel_name
        self.password = password
        self.remotehost = remotehost
        self.root_path = root_path
        self.vessel_cpus = vessel_cpus

    def gather_info(self, time:int=1000, func=None):
        res = []
        isfirst=True
        i = 0
        while i < time:
            ret = self.vessel_bash.expect([pexpect.TIMEOUT, 'RES_END'], timeout=60)
            if (ret != 1):
                self.vessel_bash.process.interact()
                raise RuntimeError("Fail to gather info")
            if not isfirst:
                curres = self.read_res(self.vessel_bash.process.before)
                if curres is None:
                    continue
                if not func is None:
                    if (func(curres)):
                        res.append(curres)
                        i += 1
                else:
                    res.append(curres)
                    i += 1
            else:
                isfirst = False
        return res

    def run_app(self, path:str, programe_cmd:str, timeout:int=30, kill_before=False):
        cli_bash = Bash(
            f'source {self.root_path}/scripts/env.rc',
            '.*SOURCE ENV DONE.', self.password, self.remotehost, False)
        cli_bash.run_cmd(
            f'export VESSEL_NAME={self.vessel_name}',
            None)
        cli_bash.run_cmd(
            f'vessel-cli {self.vessel_cpus} {path} {programe_cmd} 2>&1',
            "Sended with",
            timeout=timeout)
        cli_bash.terminate(force=True)
        cli_bash.process.wait()
    
    def close(self):
        self.vessel_bash.process.sendcontrol('c')
        self.veiokerneld_bash.process.sendcontrol('c')
        if not self.remotehost is None:
            self.vessel_bash.process.sendline('exit')
            self.veiokerneld_bash.sendline('exit')
        self.vessel_bash.terminate(force=True)
        self.veiokerneld_bash.terminate(force=True)
        self.vessel_bash.process.wait()
        self.veiokerneld_bash.process.wait()


class CCluster:
    @classmethod
    def read_res(cls, b):
        print(b)
        b = b.decode()
        if ('\n' in b):
            b = b.split('\n')
            b = b[-1]
        return json.loads(b)

    def __init__(self,
                 root_path:str='./',
                 password:str=None,
                 remotehost:str=None,
                 iokernel_cpus:str=None,
                 sched_config:str=None,
                 nicpci:str=None):
        self.iokerneld_bash = Bash(
            f"cd {root_path}",
            None,
            password, remotehost)
        if not self.iokerneld_bash.is_alive():
            raise RuntimeError(f"Fail to run bash with '{root_path}' and '{password}'")
        try:
            self.iokerneld_bash.run_cmd(
                "killall -9 iokerneld", None    
            )
            self.iokerneld_bash.run_cmd(
                f'ipcrm -a &&  ./caladan/iokerneld {sched_config} {iokernel_cpus} nicpci {nicpci} numanode 0',
                'to quit',
                30
            )
        except RuntimeError:
            self.iokerneld_bash.process.interact()
            exit()
        self.password = password
        self.remotehost = remotehost
        self.root_path = root_path
        self.bashes = dict()

    def gather_info(self, name:str, time:int=1000, func=None):
        res = []
        isfirst=True
        i = 0
        while i < time:
            ret = self.bashes[name].expect([pexpect.TIMEOUT, 'RES_END'], timeout=60)
            if (ret != 1):
                # self.bashes[name].process.interact()
                raise RuntimeError("Fail to gather info")
            if not isfirst:
                curres = self.read_res(self.bashes[name].process.before)
                if curres is None:
                    continue
                if not func is None:
                    if (func(curres)):
                        res.append(curres)
                        i += 1
                else:
                    res.append(curres)
                    i += 1
            else:
                isfirst = False
        return res

    def run_app(self, path:str, programe_cmd:str, timeout:int=30, name:str=None,kill_before:bool=False):
        cli_bash = Bash(
            None,
            None, self.password, self.remotehost, True)
        if kill_before:
            cli_bash.run_cmd(f"killall -9 {path}", None)
        cli_bash.run_cmd(f'{path} {programe_cmd}', None)

        if not name is None:
            self.bashes[name] = cli_bash
    
    def close(self):
        for key in self.bashes:
            self.bashes[key].process.sendcontrol('c')
        self.iokerneld_bash.process.sendcontrol('c')
        if not self.remotehost is None:
            for key in self.bashes:
                self.bashes[key].process.sendline('exit')
            self.iokerneld_bash.process.sendline('exit')
        for key in self.bashes:
            self.bashes[key].terminate(force=True)
        self.iokerneld_bash.terminate(force=True)
        for key in self.bashes:
            self.bashes[key].process.wait()
        self.iokerneld_bash.process.wait()

class LCluster:
    @classmethod
    def read_res(cls, b):
        #print(b)
        return json.loads(b)

    def __init__(self,
                 root_path:str='./',
                 password:str=None,
                 remotehost:str=None,
                 cpus:str=None):
        self.password = password
        self.remotehost = remotehost
        self.root_path = root_path
        self.cpus = cpus
        self.bashes = dict()

    def gather_info(self, name:str, time:int=1000, func=None):
        res = []
        isfirst=True
        i = 0
        while i < time:
            ret = self.bashes[name].expect([pexpect.TIMEOUT, 'RES_END'], timeout=60)
            if (ret != 1):
                self.bashes[name].process.interact()
                raise RuntimeError("Fail to gather info")
            if not isfirst:
                curres = self.read_res(self.bashes[name].process.before)
                if not func is None:
                    if (func(curres)):
                        res.append(curres)
                        i += 1
                else:
                    res.append(curres)
                    i += 1
            else:
                isfirst = False
        return res
    
    def gather_info_sd(self, name:str, time:int=1000, func=None, readfunc=None):
        res = []
        isfirst=True
        i = 0
        while i < time:
            ret = self.bashes[name].expect([pexpect.TIMEOUT, 'RES_END'], timeout=40)
            if (ret != 1):
                # self.bashes[name].process.interact()
                raise RuntimeError("Fail to gather info")
            if not isfirst:
                curres = readfunc(self.bashes[name].process.before)
                if curres is None:
                    continue
                if not func is None:
                    if (func(curres)):
                        res.append(curres)
                        i += 1
                else:
                    res.append(curres)
                    i += 1
            else:
                print(self.bashes[name].process.before)
                isfirst = False
        return res

    def run_app(self, path:str, programe_cmd:str, timeout:int=30, name:str=None, kill_before:bool=False, need_su:bool=True):
        root_path = ""
        if not (self.root_path is None):
            root_path = self.root_path
        cli_bash = Bash(
            f"cd {root_path}",
            None, self.password, self.remotehost, need_su)
        if kill_before:
            cli_bash.run_cmd(f"killall -w -9 {path}", None)
        cli_bash.run_cmd(f'{path} {programe_cmd}', None)
        if not name is None:
            self.bashes[name] = cli_bash
    
    def close(self):
        for key in self.bashes:
            self.bashes[key].process.sendcontrol('c')
        if not self.remotehost is None:
            for key in self.bashes:
                self.bashes[key].process.sendline('exit')
        for key in self.bashes:
            self.bashes[key].terminate(force=True)
        for key in self.bashes:
            self.bashes[key].process.wait()

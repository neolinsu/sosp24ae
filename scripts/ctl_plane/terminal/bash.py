import pexpect
import os


class Bash:
    @classmethod
    def run_cmd_in_bash(
        cls, bash, cmd: str, expect_str: str, timeout: int = 10, cmd_timeout: int = 10
    ) -> bool:
        bash.sendline(cmd)
        # Take in the cmd itself.
        index = bash.expect([pexpect.TIMEOUT, os.linesep], timeout=cmd_timeout)
        if index == 0:
            return False
        if expect_str:
            index = bash.expect([pexpect.TIMEOUT, expect_str], timeout=timeout)
            if index == 1:
                return True
            else:
                return False
        else:
            return True

    def __init__(
        self,
        set_up_bash: str = None,
        set_up_expect_str: str = None,
        password: str = None,
        remotehost: str = None,
        need_su: bool = True,
    ):
        process = pexpect.spawn("bash")
        if not remotehost is None:
            process.sendline(f"ssh {remotehost}")
            index = process.expect([pexpect.TIMEOUT, ".* password:"], timeout=20)
            if index == 0:
                raise RuntimeError("ssh remote failed")
            else:
                process.sendline(f"{password}")
        if need_su:
            process.sendline("sudo su")
            index = process.expect([pexpect.TIMEOUT, "for .*:"], timeout=3)
            if index == 0:
                pass
                # raise RuntimeError("sudo bash failed")
            else:
                process.sendline(f"{password}")
        self.process = process
        if not set_up_bash is None:
            ret = self.run_cmd_in_bash(
                process, set_up_bash, set_up_expect_str, timeout=1
            )
            if not ret:
                self.process = None

    def run_cmd(self, cmd: str, expect_str: str = None, timeout: int = 20):
        self.process.sendline(cmd)
        print(f"[RUN]: {cmd}")
        if expect_str:
            ret = self.process.expect([pexpect.TIMEOUT, expect_str], timeout=timeout)
            if ret != 1:
                self.process.interact()
                raise RuntimeError(
                    f"Fail to run bash with '{cmd}', expect '{expect_str}'."
                )
        return

    def wait_str(self, expect_str: str, timeout: int = 90):
        ret = self.process.expect([pexpect.TIMEOUT, expect_str], timeout=timeout)

        if ret != 1:
            self.process.interact()
            raise RuntimeError(f"Fail to run bash with expect '{expect_str}'.")

    def is_alive(self):
        return (
            (hasattr(self, "process"))
            and not self.process is None
            and self.process.isalive()
        )

    def expect(self, *args, **kwargs):
        return self.process.expect(*args, **kwargs)

    def terminate(self, force=False):
        return self.process.terminate(force)

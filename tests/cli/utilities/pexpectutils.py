#!/usr/bin/env python3
#
# Copyright (C) Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

import sys

if not sys.platform == "win32":
    raise ImportError("WinptySpawn is Windows-only.")

import threading
import time
from queue import Empty, Queue

from pexpect.exceptions import EOF
from pexpect.spawnbase import SpawnBase
from winpty import Backend, PtyProcess


class WinptySpawn(SpawnBase):

    def __init__(
        self,
        command,
        encoding="utf-8",
        codec_errors="strict",
        timeout=30,
        maxread=2000,
        searchwindowsize=None,
        logfile=None,
        **kwargs
    ):

        super(WinptySpawn, self).__init__(
            timeout=timeout,
            maxread=maxread,
            searchwindowsize=searchwindowsize,
            logfile=logfile,
            encoding=encoding,
            codec_errors=codec_errors,
        )

        self.pty_proc = PtyProcess.spawn(command, **kwargs, backend=Backend.ConPTY)

        self.encoding = encoding
        self._buf = self.string_type()

        self._read_queue = Queue()
        self._read_thread = threading.Thread(target=self._read_incoming, daemon=True)
        self._read_thread.start()
        self._read_reached_eof = False

    def _read_incoming(self):
        """Run in a thread to move output from a pipe to a queue."""
        while not self.pty_proc.pty.iseof():
            self._read_queue.put(self.pty_proc.read())
        self._read_queue.put(None)

    def read_nonblocking(self, size=1024, timeout=1):
        buf = self._buf
        if self._read_reached_eof:
            # We have already finished reading. Use up any buffered data,
            # then raise EOF
            if buf:
                self._buf = buf[size:]
                return buf[:size]
            else:
                self.flag_eof = True
                raise EOF("End Of File (EOF).")

        if timeout == -1:
            timeout = self.timeout
        elif timeout is None:
            timeout = 1e6

        t0 = time.time()
        while (time.time() - t0) < timeout and size and len(buf) < size:
            try:
                incoming = self._read_queue.get_nowait()
            except Empty:
                break
            else:
                if incoming is None:
                    self._read_reached_eof = True
                    break

                buf += incoming

        r, self._buf = buf[:size], buf[size:]
        self._log(r, "read")
        return r

    def write(self, s):
        if isinstance(s, str):
            s = s.encode(self.encoding)
        self.pty_proc.write(s.decode(self.encoding))

    def flush(self):
        pass  # Not needed for pywinpty

    def terminate(self, force=False):
        try:
            self.pty_proc.write("exit\r\n")
            self.pty_proc.close()
        except Exception:
            pass

    def isalive(self):
        return self.pty_proc.isalive()

    def send(self, s):
        self._log(s, "send")
        self.write(s)

    def sendline(self, s=""):
        self.write(s + "\n")

    def close(self):
        self.terminate()

    def wait(self):
        """Wait for the subprocess to finish.

        Returns the exit code.
        """
        status = self.pty_proc.wait()
        if status >= 0:
            self.exitstatus = status
            self.signalstatus = None
        else:
            self.exitstatus = None
            self.signalstatus = -status
        self.terminated = True
        return status

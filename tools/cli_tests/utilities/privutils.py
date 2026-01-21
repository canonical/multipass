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

import functools
import inspect
import logging
import os
import shutil
import site
import subprocess
import sys
from pathlib import Path

import pytest


@functools.cache
def get_sudo_tool():
    if sys.platform == "win32":
        sudo_tool = "gsudo"
        install_hint = "winget install gsudo"
        default_args = ["--direct"]
    else:
        sudo_tool = "sudo"
        install_hint = "apt install sudo"
        default_args = []

    result = shutil.which(sudo_tool)

    if not result:
        pytest.exit(
            f"❌ `{sudo_tool}` is required but not found. Install it with: `{install_hint}`",
            returncode=10,
        )

    return [result] + default_args


def sudo(*args):
    return [*get_sudo_tool(), *args]


def run_in_new_interpreter(
    py_func, *args, privileged=False, check=False, **kwargs
):
    """
    Run a top-level function with elevated privileges using sudo.

    The function must be defined in an importable module (not __main__ or REPL).
    PYTHONPATH is set to include the module's root and the user's site-packages
    so that imports work correctly under sudo.

    Args:
        py_func: The function to run.
        *args: Arguments to pass to the function.
        check: If True, raise an error if the subprocess fails.
        stdout: Where to redirect standard output (default: inherit).
        stderr: Where to redirect standard error (default: inherit).
        privileged: Whether the subprocess should be run with elevated privileges. (default: False)
    """
    if not callable(py_func):
        raise ValueError("Expected a callable")

    def infer_pythonpath_root(module):
        depth = len(module.__name__.split(".")) - 1
        logging.debug(f"depth: {depth}")
        return str(Path(module.__file__).resolve().parents[depth])

    module = inspect.getmodule(py_func)

    if module is None or not hasattr(module, "__file__"):
        raise ValueError(
            "Function must come from an importable module (not REPL or __main__)"
        )

    module_name = module.__name__
    func_name = py_func.__name__
    module_root = infer_pythonpath_root(module)

    logging.debug(f"module.__file__ = {module.__file__}")
    logging.debug(f"inferred PYTHONPATH = {module_root}")
    # Positional args to string literals
    arg_strs = [repr(a) for a in args]
    # print('🔍 sys.path:', sys.path);
    full_expr = f"import sys; import {module_name}; {module_name}.{func_name}({', '.join(arg_strs)})"
    env = os.environ.copy()
    env["PYTHONPATH"] = (
        # Before running the executable, we need to add two things to PYTHONPATH:
        # 1) The test package's root directory
        # 2) Current user's site-packages
        # '2' is required since test dependencies are most likely installed at
        # the user level. If not, root/admin's site-packages are already accessible
        # to root by default.
        f"{module_root}{os.pathsep}{site.getusersitepackages()}{os.pathsep}{env.get('PYTHONPATH', '')}"
    )

    cmd_prologue = []
    if privileged:
        cmd_prologue = [
            *get_sudo_tool(),
            "--preserve-env"
            if sys.platform == "win32"
            else "--preserve-env=PYTHONPATH",
        ]

    cmd = [
        sys.executable,
        "-c",
        full_expr,
    ]

    return subprocess.run(
        [*cmd_prologue, *cmd], check=check, **kwargs, env=env
    )

async def run_in_new_interpreter_async(*a, **kw):
    import asyncio
    return await asyncio.to_thread(run_in_new_interpreter, *a, **kw)
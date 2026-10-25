# SPDX-License-Identifier: GPL-3.0-only
from bootstrap_ps5_toolchain import environment, host_options, prepare
from ps5_common import CACHE, ROOT, run

prefix = prepare()
env = environment(prefix)
flags, link = host_options(prefix)
output = CACHE / "platform-tests"
output.mkdir(parents=True, exist_ok=True)
objects = []
for name in ("alloc", "vout"):
    obj = output / (name + ".o")
    run([prefix / "usr/bin/clang-18", *flags, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
         "-DGS_NATIVE", "-c", ROOT / "native/ps5/platform" / (name + ".c"), "-o", obj], env=env)
    objects.append(obj)
exe = output / "platform-tests"
run([prefix / "usr/bin/clang++-18", *flags, *link, "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
     ROOT / "native/tests/ps5_platform_tests.cpp", *objects, "-pthread", "-o", exe], env=env)
run([exe], env=env)

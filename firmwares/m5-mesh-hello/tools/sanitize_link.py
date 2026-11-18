# SPDX-License-Identifier: GPL-3.0-or-later
# PlatformIO extra script for env:native_sanitize: passes the sanitizer flags
# to the linker as well (build_flags alone only reach the compiler).
Import("env")  # noqa: F821  (injected by PlatformIO/SCons)

env.Append(LINKFLAGS=["-fsanitize=address,undefined", "-fno-sanitize-recover=all"])  # noqa: F821

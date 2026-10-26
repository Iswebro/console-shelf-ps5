# SPDX-License-Identifier: GPL-3.0-only
"""Cross-compile Shelf in the project cache. Does not connect to a console."""
import json
import hashlib
from bootstrap_ps5_toolchain import environment, host_options, prepare as prepare_compiler
from fetch_ps5_deps import prepare as prepare_dependencies
from ps5_common import CACHE, ROOT, run, sha256
from verify_ps5_build import require, source_fingerprint, verify
from build_launch_helper import main as build_launch_helper


def cached_run(args, inputs, output, env):
    record = {"args": [str(a) for a in args], "inputs": {str(p): sha256(p) for p in sorted(inputs)}}
    key = hashlib.sha256(json.dumps(record, sort_keys=True).encode()).hexdigest()
    stamp = output.with_name(output.name + ".build.json")
    if output.is_file() and stamp.is_file():
        old = json.loads(stamp.read_text())
        if old.get("key") == key and old.get("sha256") == sha256(output):
            return
    run(args, env=env)
    stamp.write_text(json.dumps({"key": key, "sha256": sha256(output)}, indent=2) + "\n")


def build(standard_runtime=False):
    identity = source_fingerprint()
    video_sources = ROOT / "native/third_party/pl_mpeg"
    for entry in json.loads((video_sources / "SOURCE.json").read_text())["files"]:
        source = video_sources / entry["path"]
        require(source.stat().st_size == entry["bytes"] and sha256(source) == entry["sha256"],
                "PL_MPEG source differs from the recorded upstream pin")
    prefix = prepare_compiler()
    env = environment(prefix)
    sdk, ports, zlib = prepare_dependencies()
    output = CACHE / "build"
    output.mkdir(parents=True, exist_ok=True)
    build_launch_helper(standard_runtime)
    helper_folder = CACHE / ("launch-helper-standard" if standard_runtime else "launch-helper")
    helper = helper_folder / "shelf-launch.elf"
    helper_record = json.loads((helper_folder / "build.json").read_text())
    require(helper_record["elf"] == {"bytes": helper.stat().st_size, "sha256": sha256(helper)}, "Helper build changed")
    payload = helper.read_bytes()
    literal = ",\n".join(",".join(f"0x{byte:02x}" for byte in payload[start:start + 24]) for start in range(0, len(payload), 24))
    (output / "shelf_launch_asset.hpp").write_text(
        "// Generated from the verified owned helper; never read from a console or external request.\n"
        "static const unsigned char shelf_launch_payload[] = {\n" + literal + "\n};\n")
    runtime = ROOT / "native/third_party/ps5-runtime/tooling/native"
    manifest = json.loads((runtime.parent.parent / "SOURCE.json").read_text())
    for entry in manifest["files"]:
        source = runtime.parent.parent / entry["path"]
        if source.stat().st_size != entry["bytes"] or sha256(source) != entry["sha256"]:
            raise RuntimeError(f"Imported runtime source fails its pin: {entry['path']}")
    compiler = prefix / "usr/bin/clang-18"
    cxx = prefix / "usr/bin/clang++-18"
    host_flags, host_link = host_options(prefix)
    # Build the host's zlib locally from its pinned source; never use a target lib.
    local_zlib = output / "zlib"
    local_zlib.mkdir(exist_ok=True)
    z_sources = ["adler32", "crc32", "deflate", "infback", "inffast", "inflate", "inftrees", "trees",
                 "zutil", "compress", "uncompr", "gzclose", "gzlib", "gzread", "gzwrite"]
    z_objects = []
    for name in z_sources:
        obj = local_zlib / (name + ".o")
        cached_run([compiler, *host_flags, "-O2", "-DHAVE_UNISTD_H", "-I", zlib, "-c", zlib / (name + ".c"), "-o", obj],
                   [zlib / (name + ".c"), *zlib.glob("*.h")], obj, env)
        z_objects.append(obj)
    run(["ar", "rcs", local_zlib / "libz.a", *z_objects], env=env)
    tool = output / "ps5-native-tool"
    cached_run([cxx, *host_flags, *host_link, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-I", zlib,
         *(runtime / (name + ".cpp") for name in ("native_app_builder", "elf_object", "self_container", "sce_module_writer")),
         local_zlib / "libz.a", "-o", tool], [*runtime.glob("*.cpp"), *runtime.glob("*.hpp"),
                                             zlib / "zlib.h", zlib / "zconf.h", local_zlib / "libz.a"], tool, env)
    libc_builder = output / "libc-builder"
    cached_run([cxx, *host_flags, *host_link, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                runtime / "libc_builder.cpp", "-o", libc_builder], [runtime / "libc_builder.cpp"], libc_builder, env)
    # Reproduce, rather than copy, the reference's clean-room runtime.
    for copy in ("a", "b"):
        raw = output / f"libc-{copy}.elf"
        signed = output / f"libc-{copy}.prx"
        run([libc_builder, runtime / "runtime/api-surface.txt", runtime / "runtime/imports.txt", raw], env=env)
        if sha256(raw) != "8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb":
            raise RuntimeError("Rebuilt raw clean-room runtime does not match the reference")
        run([tool, "self", "--sign", "--in", raw, "--out", signed], env=env)
        if sha256(signed) != "e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036":
            raise RuntimeError("Rebuilt signed clean-room runtime does not match the reference")
    print("Clean-room runtime rebuilt twice with the exact reference SHA-256.", flush=True)
    common = ["-target", "x86_64-sie-ps5", "-fvisibility-nodllstorageclass=default", "-isysroot", sdk,
              "-isystem", sdk / "target/include/c++/v1", "-isystem", sdk / "target/include",
              "-fno-stack-protector", "-fno-plt", "-femulated-tls", "-O2", "-fno-omit-frame-pointer", "-ffunction-sections", "-fdata-sections",
              "-DSHELF_PS5", "-DGS_NATIVE", "-DNDEBUG", "-D_REENTRANT", "-D_THREAD_SAFE",
              "-I", ROOT / "native/include", "-I", ROOT / "native/ps5", "-I", ROOT / "native/third_party", "-I", output,
              "-I", ports / "include/SDL2", "-I", ports / "include"]
    sources = [ROOT / "native/src" / (name + ".cpp") for name in ("model", "settings", "ui", "graphics", "codecs", "movie", "library", "artwork_sources", "artwork", "filesystem", "metadata", "ftp", "launch_client", "host_main")]
    sources += [ROOT / "native/src/json.c"]
    sources += [ROOT / "native/ps5/platform" / name for name in ("alloc.c", "vout.c", "shims.c", "lifecycle.c")]
    sources += [runtime / "app_crt.cpp", runtime / "app_cpp_runtime.cpp"]
    objects = []
    for source in sources:
        obj = output / (source.stem + ".o")
        language = ["-std=c++17", "-fno-exceptions", "-fno-rtti"] if source.suffix == ".cpp" else ["-std=c11"]
        warnings = ["-Wall", "-Wextra", "-Werror", "-Wno-unused-function"]
        if source.stem == "codecs":
            warnings = ["-w"]  # Unmodified third-party implementations; owned units remain strict.
        print("Compile PS5:", source.name, flush=True)
        run([compiler, *common, *language, *warnings, "-c", source, "-o", obj], env=env)
        objects.append(obj)
    # Equivalent to SDK v0.42's prospero-lld wrapper, using the pinned linker
    # directly so no global llvm-config symlink or installation is needed.
    pie = output / "shelf-pie.elf"
    run([prefix / "usr/bin/ld.lld-18", "-m", "elf_x86_64", "-pie", "-z", "max-page-size=0x4000",
         "-mllvm", "-emulated-tls", "--hash-style=gnu", "--eh-frame-hdr", "-T", runtime / "ps5-pie.ld",
         "--version-script", runtime / "app-symbols.map", "-e", "_start", "-o", pie, *objects,
         "--start-group", *(ports / "lib" / name for name in ("libSDL2.a", "libiconv.a", "libsamplerate.a")),
         "--end-group", "--as-needed", *sorted((sdk / "target/lib").glob("*.so"))], env=env)
    elf = output / "eboot.elf"
    run([tool, "link", "--in", pie, "--out", elf, "--stub-dir", sdk / "target/lib",
         "--module-sdk", "0x02000009", "--companion-sdk", "0x08050001", "--file-name", "eboot.elf"], env=env)
    signed = output / "eboot.bin"
    run([tool, "self", "--sign", "--in", elf, "--out", signed, "--magic", "0x1D3D154F"], env=env)
    run([tool, "self", "--inspect", "--file", signed], env=env)
    report = verify(elf, signed)
    require(elf.read_bytes().count(payload) == 1, "The verified helper is not embedded exactly once")
    report["embeddedLaunchHelper"] = {**helper_record["elf"], "offset": elf.read_bytes().index(payload),
                                     "runtimeMode": helper_record["runtimeMode"],
                                     "permissionChanges": helper_record["permissionChanges"]}
    require(identity == source_fingerprint(), "Build inputs changed during compilation; rebuild before verification")
    (output / "build-source.json").write_text(json.dumps(identity, indent=2) + "\n")
    (output / "structural-check.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"PS5 executable compiled at {signed}; NOT a validated console package.", flush=True)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--standard-helper-runtime", action="store_true",
                        help="Stage a build with the SDK's permission-changing startup; does not deploy or run it")
    build(parser.parse_args().standard_helper_runtime)

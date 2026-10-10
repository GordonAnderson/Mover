Import("env")
import re
import os
import shutil

# Post build step: copies the built .bin to firmware/Mover-R<version>.bin, taking the
# version number from the Version string in src/Mover.cpp.


def rename_firmware(source, target, env):
    src_dir     = env.subst("$PROJECT_SRC_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    mover_cpp    = os.path.join(src_dir, "Mover.cpp")

    # Pull the version number from Mover.cpp  e.g. #define MOVER_VERSION "Mover R1.1 October 10, 2026"
    version = "unknown"
    try:
        with open(mover_cpp) as f:
            for line in f:
                m = re.search(r'MOVER_VERSION\s+"Mover\s+R([\d.]+)', line)
                if m:
                    version = m.group(1)
                    break
    except Exception as e:
        print(f"rename_firmware: could not read Mover.cpp: {e}")

    built_bin = str(target[0])
    out_dir   = os.path.join(project_dir, "firmware")
    out_name  = f"Mover-R{version}.bin"
    out_path  = os.path.join(out_dir, out_name)

    try:
        os.makedirs(out_dir, exist_ok=True)
        shutil.copy(built_bin, out_path)
        print("")
        print("=" * 50)
        print(f"  Firmware copied to: firmware/{out_name}")
        print("=" * 50)
        print("")
    except Exception as e:
        print(f"rename_firmware: could not copy firmware: {e}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", rename_firmware)

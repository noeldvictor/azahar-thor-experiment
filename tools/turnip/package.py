"""Package a Turnip build as an adrenotools driver zip.

Usage: package.py <libvulkan_freedreno.so> <out.zip> <mesa-version> <package-revision>
"""
import json
import sys
import zipfile


def main() -> None:
    library, out, mesa_version, revision = sys.argv[1:5]
    meta = {
        "schemaVersion": 1,
        "name": f"Turnip Thor {mesa_version} r{revision}",
        "description": (
            "Mesa Turnip built for the AYN Thor: RB registers confined to the BR pipe "
            "(tools/turnip/patches)."
        ),
        "author": "azahar-thor",
        "packageVersion": revision,
        "vendor": "Mesa",
        "driverVersion": f"Vulkan 1.4 (Mesa {mesa_version})",
        "minApi": 27,
        "libraryName": "vulkan.ad07xx.so",
    }
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zip_file:
        zip_file.writestr("meta.json", json.dumps(meta, indent=2))
        zip_file.write(library, "vulkan.ad07xx.so")
    print(out)


if __name__ == "__main__":
    main()

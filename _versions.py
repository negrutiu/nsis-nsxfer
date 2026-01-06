import subprocess
from subprocess import Popen, PIPE
from datetime import datetime, timezone
import re
import os

scriptdir = os.path.dirname(os.path.abspath(__file__))

def replace_resource_version(file, file_version=None, prod_version=None, update_copyright_year=True):
    """ Replace version fields in resource (*.rc) file """
    regexes = {}
    if file_version:
        v = file_version.split('.')
        if len(v) != 4: raise ValueError(f"invalid file version {file_version}")
        regexes |= {
            re.compile(r'^\s*FILEVERSION\s+([\d \t,]+)*$') : f"{v[0]},{v[1]},{v[2]},{v[3]}",
            re.compile(r'^\s*VALUE\s+"FileVersion"\s*,\s*"([\d\.]+)"\s*$') : f"{v[0]}.{v[1]}.{v[2]}.{v[3]}",
        }
    if prod_version:
        v = prod_version.split('.')
        if len(v) != 4: raise ValueError(f"invalid product version {prod_version}")
        regexes |= {
            re.compile(r'^\s*PRODUCTVERSION\s+([\d \t,]+)*$') : f"{v[0]},{v[1]},{v[2]},{v[3]}",
            re.compile(r'^\s*VALUE\s+"ProductVersion"\s*,\s*"([\d\.]+)"\s*$') : f"{v[0]}.{v[1]}.{v[2]}.{v[3]}",
        }
    if update_copyright_year:
        regexes |= { re.compile(r'^\s*VALUE\s+"LegalCopyright"\s*,\s*"\D+\d+-(\d+).*$') : f"{datetime.now().year}" }

    lines = []
    replaced = 0

    with open(file) as infile:
        print(f"-- replacing in \"{infile.name}\":")
        for line in infile:
            found = False
            for regex, value in regexes.items():
                groups = regex.match(line)
                if groups != None:
                    found = True
                    line2 = line.replace(groups[1], value)
                    if line != line2:
                        print(f"replaced \"{groups[1]}\" with \"{value}\":")
                        print("  {}".format(line2.replace('\n', '')))
                        replaced = replaced + 1
                    lines.append(line2)
                    break
            if not found:
                lines.append(line)

    if replaced > 0:
        with open(infile.name, 'w') as outfile:
            for line in lines:
                # print("{}".format(line.replace('\n', '')))
                outfile.write(line)
    else:
        print("resource file already up-to-date")


def get_gcc_version(gcc='gcc'):
    """ Query `gcc` version """
    process = Popen([gcc, "-v"], stdout=PIPE, stderr=PIPE)
    (cout, cerr) = process.communicate()
    exit_code = process.wait()

    # possible examples:
    # "gcc version 14.1.0 (Rev3, Built by MSYS2 project)"
    # "gcc version 13.1.0 (MinGW-W64 x86_64-msvcrt-posix-seh, built by anonymous)"
    # "gcc version 14.2.0 (MinGW-W64 i686-ucrt-posix-dwarf, built by Brecht Sanders, r3)"
    # "gcc version 15.2.0 (i686-posix-dwarf-rev0, Built by MinGW-Builds project)"
    # "gcc version 15.2.0 (GCC)"

    if cerr != None:
        for line in cerr.decode('utf-8').splitlines():
            # print(f"cerr | {line}", flush=True)
            if match := re.match(r'^gcc version (.+)\s\(', line):
                version = match[1]
                for revision in [r'\(Rev(\d+),', r'-rev(\d+),', r', r(\d+)\)']:
                    if match := re.search(revision, line, re.IGNORECASE):
                        version += ('-' + match[1]) if int(match[1]) != 0 else ''
                        break
                return version
    return ""


def command_replace_resource(args):
    replace_resource_version(file=args.file, file_version=args.version, prod_version=args.version)

def command_gcc_version(args):
    if os.path.isabs(args.prefix):
        gcc = os.path.join(args.prefix, "gcc")
    else:
        gcc = args.prefix + "gcc"
    if version := get_gcc_version(gcc):
        print(version)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(help='available commands')

    date = datetime.now(tz=timezone.utc)
    default_version = f"{date.year}.{date.month}.{date.day}.0"
    default_file = os.path.join(scriptdir, "resource.rc")

    parser_replace_rc = subparsers.add_parser('replace-rc', help='replace version fields in resource file (*.rc)')
    parser_replace_rc.add_argument("-f", "--file", type=str, default=default_file, help=f"resource file to update (default: {default_file})")
    parser_replace_rc.add_argument("-v", "--version", type=str, default=default_version, help=f"version to set (default: {default_version})")
    parser_replace_rc.set_defaults(func=command_replace_resource)

    parser_gcc_version = subparsers.add_parser('gcc', help='get gcc version (e.g. "15.2.0-8")')
    parser_gcc_version.add_argument("-p", "--prefix", type=str, default="", help='gcc prefix (e.g., "x86_64-w64-mingw32-", "C:\\mingw64\\bin")')
    parser_gcc_version.set_defaults(func=command_gcc_version)

    args = parser.parse_args()

    if hasattr(args, 'func'):
        args.func(args)
    else:
        parser.print_help()

import subprocess
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


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", type=str, default=os.path.join(scriptdir, "resource.rc"))
    parser.add_argument("-v", "--version", type=str, default=None)
    args = parser.parse_args()

    if not args.version:
        date = datetime.now(tz=timezone.utc)
        args.version = f"{date.year}.{date.month}.{date.day}.0"
    
    replace_resource_version(file=args.file, file_version=args.version, prod_version=args.version)

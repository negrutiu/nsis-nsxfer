import os, shutil

scriptdir = os.path.abspath(os.path.dirname(__file__))

def make_package(packdir, packzip=None):
    shutil.rmtree(packdir, ignore_errors=True)
    files = {
        os.path.join('build', 'Release-mingw-amd64-unicode', 'NSxfer.dll'): os.path.join('amd64-unicode', 'NSxfer.dll'),
        os.path.join('build', 'Release-mingw-x86-unicode',   'NSxfer.dll'): os.path.join('x86-unicode', 'NSxfer.dll'),
        os.path.join('build', 'Release-mingw-x86-ansi',      'NSxfer.dll'): os.path.join('x86-ansi', 'NSxfer.dll'),
        'NSxfer.Readme.txt': 'NSxfer.Readme.txt',
        'README.md':         'README.md',
        'LICENSE.md':        'LICENSE.md',
    }
    for src, dst in files.items():
        srcpath = os.path.join(scriptdir, src)
        dstpath = os.path.join(packdir, dst)
        os.makedirs(os.path.dirname(dstpath), exist_ok=True)
        os.link(srcpath, dstpath)
    if packzip:
        if os.path.splitext(packzip)[1] == '.zip':
            packzip = os.path.splitext(packzip)[0]  # remove .zip
        shutil.make_archive(packzip, 'zip', packdir)

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--outdir", type=str, default=os.path.join(scriptdir, "build", "package"))
    parser.add_argument("-z", "--outzip", type=str, default=None)
    args = parser.parse_args()

    make_package(args.outdir, args.outzip)
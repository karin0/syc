import os
import shutil
import subprocess
import threading
import multiprocessing
import concurrent.futures as futures

path = os.path
rpath = path.realpath

project_dir = '..'
mars_path = '../../mars.jar'
gcc = 'gcc'
compile_timeout = 5
mars_timeout = 5

header_path = rpath('my.h')
syc_path = rpath('build/syc')
project_dir = rpath(project_dir)
mars_path = rpath(mars_path)
cpu_count = multiprocessing.cpu_count()

stats = []

def put_stat(src_file, stat):
    if stat.startswith('div'):
        stats.append((src_file, stat))  # we assume this is atomic
    elif stats:
        raise RuntimeError


def call(args):
    subprocess.run(args, check=True)


def build():
    if not path.isdir('build'):
        os.mkdir('build')
    os.chdir('build')
    call(['cmake', project_dir, '-DCMAKE_BUILD_TYPE=Release'])
    call(['make', f'-j{cpu_count}'])
    os.chdir('..')


def _run(rid, src_file, in_file, ans_file):
    iden = src_file

    src_file = rpath(src_file)
    in_file = rpath(in_file)
    ans_file = rpath(ans_file)

    rid = str(rid)
    if not path.isdir(rid):
        os.mkdir(rid)
    os.chdir(rid)

    print('testing', iden)
    subprocess.run(
        [syc_path, src_file, '-o', 'out.asm'],
        timeout=compile_timeout,
        check=True
    )

    if not in_file:
        print('no input file')

    with open(in_file, encoding='utf-8') as fp:
        with open('in.txt', 'w', encoding='utf-8') as in_fp:
            for line in fp:
                for x in line.split():
                    in_fp.write(x)
                    in_fp.write('\n')

    with open(in_file, 'rb') as in_fp:
        with open('out.txt', 'wb') as out_fp:
            stat = subprocess.run(
                ['java', '-jar', mars_path, 'nc', 'me', 'mc', 'Default', 'out.asm'],
                stdin=in_fp,
                stdout=out_fp,
                stderr=subprocess.PIPE,
                check=True,
                timeout=mars_timeout
            ).stderr
            if isinstance(stat, bytes):
                stat = stat.decode('utf-8').lower()
            put_stat(src_file, stat)

        if not ans_file:
            ans_file = 'ans.txt'
            shutil.copy(header_path, 'src.c')
            with open('src.c', 'ab') as src_fp:
                with open(src_file, 'rb') as fp:
                    src_fp.write(fp.read())

            call([gcc, '-O0', '-x', 'c', '-o', 'a.out', 'src.c'])
            with open('ans.txt', 'wb') as ans_fp:
                subprocess.run(
                    ['./a.out'],
                    stdin=in_fp,
                    stdout=ans_fp,
                    check=True
                )

    with open(ans_file, 'rb') as fp:
        ans_bytes = fp.read()

    with open('out.txt', 'rb') as fp:
        out_bytes = fp.read()

    if ans_bytes != out_bytes:
        raise RuntimeError('diff')
    print('ok')


fail_evt = threading.Event()


def run(src_file, in_file, ans_file):
    if fail_evt.is_set():
        return
    try:
        return _run(src_file, in_file, ans_file)
    except BaseException as e:
        fail_evt.set()
        raise e


def find_cases(dir):
    res = []
    for root, _, fns in os.walk(dir):
        sfns = None
        for fn in fns:
            if fn.startswith('testfile'):
                fin = 'input' + fn[8:]
                if sfns is None:
                    sfns = frozenset(fns)
                if fin in sfns:
                    res.append((path.join(root, fn), path.join(root, fin), None))
    return res


def get_cases():
    # return find_cases(cases_root)


def main():
    build()

    cases = get_cases()
    with futures.ThreadPoolExecutor(max_workers=cpu_count) as executor:
        futs = [executor.commit(run, i, *case) for i, case in enumerate(cases)]
        for fut in futures.as_completed(futs):
            fut.result()

    print(stats)

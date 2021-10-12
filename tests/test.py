import os
import csv
import time
import shutil
import subprocess
import threading
import multiprocessing
import concurrent.futures as futures
from datetime import datetime

cases_root = '/home/karin0/lark/buaa/ct/cases'
project_dir = '/home/karin0/lark/buaa/ct/syc'
mars_path = '/home/karin0/lark/buaa/ct/mars.jar'
results_file = 'results.csv'

diff = ['fc'] if os.name == 'nt' else ['diff', '-b']
cmake = 'cmake'
make = 'make'
gcc = 'gcc'
compile_timeout = 5
mars_timeout = 5

path = os.path
os.chdir(path.dirname(path.realpath(__file__)))

header_path = 'my.h'
syc_path = path.realpath('build/syc')
cpu_count = multiprocessing.cpu_count()

stats = []


def put_stat(src_file, stat):
    if stat.startswith('div'):
        stats.append((src_file, stat))  # we assume this is atomic
    elif stat:
        raise RuntimeError('bad mars stat: ' + stat)
    elif stats:
        raise RuntimeError


def call(args):
    subprocess.run(args, check=True)


def mkdir(s):
    if not path.isdir(s):
        os.mkdir(s)


def build():
    mkdir('build')
    dir = path.realpath(project_dir)
    os.chdir('build')
    call([cmake, dir, '-DCMAKE_BUILD_TYPE=Release'])
    call([make, f'-j{cpu_count}'])
    os.chdir('..')


mkdir('out')


def _run(rid, src_file, in_file, ans_file):
    iden = path.relpath(src_file, cases_root)
    ridx = rid

    def msg(*a):
        print(f'[{ridx}] {iden}: ', end='')
        print(*a)

    rid = f'out/{rid}'
    mkdir(rid)

    asm_fn = path.join(rid, 'out.asm')
    in_fn = path.join(rid, 'in.txt')
    out_fn = path.join(rid, 'out.txt')
    gcc_src_fn = path.join(rid, 'src.c')
    gcc_out_fn = path.join(rid, 'a.out')

    msg('compiling')
    subprocess.run(
        [syc_path, src_file, '-o', path.realpath(asm_fn)],
        timeout=compile_timeout,
        check=True,
        cwd=rid
    )

    if not in_file:
        msg('no input file')

    with open(in_file, encoding='utf-8') as fp:
        with open(in_fn, 'w', encoding='utf-8') as in_fp:
            for line in fp:
                for x in line.split():
                    in_fp.write(x)
                    in_fp.write('\n')

    with open(in_fn, 'rb') as in_fp:
        with open(out_fn, 'wb') as out_fp:
            msg('simulating')
            stat = subprocess.run(
                ['java', '-jar', mars_path, 'nc', 'me', 'mc', 'Default', asm_fn],
                stdin=in_fp,
                stdout=out_fp,
                stderr=subprocess.PIPE,
                check=True,
                timeout=mars_timeout
            ).stderr
            if isinstance(stat, bytes):
                stat = stat.decode('utf-8')
            stat = stat.lower().strip()
            msg(stat)
            put_stat(src_file, stat)

        if not ans_file:
            ans_file = path.join(rid, 'ans.txt')
            shutil.copy(header_path, gcc_src_fn)
            with open(gcc_src_fn, 'ab') as src_fp:
                with open(src_file, 'rb') as fp:
                    src_fp.write(fp.read())

            call([gcc, '-O0', '-x', 'c', '-o', gcc_out_fn, gcc_src_fn])
            in_fp.seek(0)
            with open(ans_file, 'wb') as ans_fp:
                subprocess.run(
                    [gcc_out_fn],
                    stdin=in_fp,
                    stdout=ans_fp,
                    check=True
                )

    '''
    with open(ans_file, 'rb') as fp:
        ans_bytes = fp.read()

    with open(out_fn, 'rb') as fp:
        out_bytes = fp.read()

    if ans_bytes != out_bytes:
        raise RuntimeError(f'diff {ans_file} {out_fn}')
    '''
    call(diff + [ans_file, out_fn])
    msg('ok')


fail_evt = threading.Event()


def run(rid, *args):
    if fail_evt.is_set():
        return
    try:
        return _run(rid, *args)
    except BaseException as e:
        fail_evt.set()
        print(f'[{rid}] {type(e).__name__} {e}')
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
    return find_cases(cases_root)


def write_csv(fn, data):
    with open(fn, 'w', encoding='utf-8', newline='') as fp:
        w = csv.writer(fp)
        for row in data:
            w.writerow(row)


def read_csv(fn):
    with open(fn, encoding='utf-8', newline='') as fp:
        return list(csv.reader(fp))


def case_iden(s):
    s = path.relpath(s, cases_root)
    a = path.normpath(s).split(os.sep)
    if '926' in s:
        return a[-2] + '-' + a[-1][8:-4]
    elif 'hw' in s:
        return 'hw-' + a[-1][8:-4]
    else:
        return s


def key0(a):
    s = a[0]
    p = s.rfind('-')
    if p >= 0:
        try:
            x = int(s[p + 1:])
            return s[:p], x
        except ValueError:
            pass
    return s, 0


def main():
    now = datetime.now()
    print(cpu_count, 'cores found')
    build()

    cases = get_cases()
    with futures.ThreadPoolExecutor(max_workers=cpu_count) as executor:
        dt = time.time()
        for fut in futures.as_completed([
            executor.submit(run, i, *case)
            for i, case in enumerate(cases)
        ]):
            fut.result()
        dt = time.time() - dt

    if stats:
        fn = now.strftime('result_%Y-%m-%d_%H-%M-%S.csv')
        head = ['Case']
        for s in stats[0][1].split()[0::2]:
            head.append(s)

        sts = [(case_iden(src), stat.split()[1::2]) for src, stat in stats]
        sts.sort(key=key0)

        if len(frozenset(x for x, y in sts)) != len(sts):
            raise RuntimeError('case iden conflicts')

        data = [head] + [[src] + stat for src, stat in sts]
        write_csv(fn, data)

        now = now.strftime('%m-%d %H:%M:%S')
        sts = [(src, stat[-2]) for src, stat in sts]

        if path.isfile(results_file):
            shutil.copy2(results_file, results_file + '.old.csv')
            old_data = read_csv(results_file)
            row_map = {}
            for row in old_data[1:]:
                src = row[0]
                row_map[src] = row
            w = len(old_data[0]) - 1
            for src, fc in sts:
                row = row_map.get(src)
                if row is None:
                    row_map[src] = [src] + [''] * w + [fc]
                else:
                    row.append(fc)
            rows = list(row_map.values())
            rows.sort(key=key0)
            data = [old_data[0] + [now]] + rows
        else:
            data = [['Case', now]] + [[src, fc] for src, fc in sts]

        write_csv(results_file, data)

    print(f'{dt:.3} secs elapsed, {dt / len(cases):.3} secs per case')


if __name__ == '__main__':
    main()

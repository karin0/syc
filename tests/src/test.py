import sys
import time
import threading
import concurrent.futures as futures
from datetime import datetime
from env import *

now = datetime.now()
stats = []


def build():
    mkdir('build')
    dir = path.realpath(project_dir)
    os.chdir('build')
    try:
        call([cmake, dir, '-DCMAKE_BUILD_TYPE=Release'])
    except subprocess.CalledProcessError:
        os.chdir('..')
        shutil.rmtree('build')
        os.mkdir('build')
        os.chdir('build')
        call([cmake, dir, '-DCMAKE_BUILD_TYPE=Release'])

    call([make, f'-j{cpu_count}'])
    os.chdir('..')


def _run(rid, src_file, in_file, ans_file, runner=default_runner):
    iden = path.relpath(src_file, cases_root)
    ridx = rid

    def msg(*a):
        print(f'[{ridx}] {iden}: ', end='')
        print(*a)

    rid = f'out/{rid}'
    mkdir(rid)

    for fn in os.listdir(rid):
        os.remove(path.join(rid, fn))

    if src_file:
        case_fn = path.join(rid, 'case.txt')
        msg(f'src copied to {shutil.copy(src_file, case_fn)}')

    stat = runner(rid, src_file, in_file, ans_file, msg)
    if stat:
        stats.append(stat)


fail_lock = threading.Lock()
fail_rid = None


def run(rid, *args):
    global fail_rid
    with fail_lock:
        if fail_rid is not None:
            return
    try:
        return _run(rid, *args)
    except BaseException as e:
        acquired = False
        with fail_lock:
            if fail_rid is None:
                fail_rid = rid
                acquired = True
        pre = f'[{rid}] ({args[0]})'
        print(pre, f'{type(e).__name__} {e}')
        if acquired:
            print(pre, 'acquired fail-out')
            if path.isdir('fail-out'):
                if path.isdir('fail-out-old'):
                    shutil.rmtree('fail-out-old')
                shutil.move('fail-out', 'fail-out-old')
            shutil.copytree(f'out/{rid}', 'fail-out')
        raise e


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
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        fail = 'f' in arg
        no_stat = fail or 'n' in arg
        if not (fail or no_stat):
            raise ValueError('invalid args')
    else:
        fail = no_stat = False

    print(cpu_count, 'cores found')

    os.chdir(path.join(file_dir, '..'))
    build()

    if fail:
        cases = get_fail_cases()
    else:
        cases = get_cases()

    mkdir('out')

    with futures.ThreadPoolExecutor(max_workers=cpu_count) as executor:
        dt = time.time()
        for fut in futures.as_completed([
            executor.submit(run, i, *case)
            for i, case in enumerate(cases)
        ]):
            fut.result()
        dt = time.time() - dt

    if not no_stat and stats:
        mkdir('stats')
        os.chdir('stats')

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

        tim = now.strftime('%m-%d %H:%M:%S')
        sts = [(src, stat[-2]) for src, stat in sts]

        if path.isfile(results_file):
            shutil.copy2(results_file, results_file + '.old.csv')
            old_data = read_csv(results_file)
            # TODO: this logic needs fixing, when some row has the last column empty
            # TODO: render diff and sum
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
            data = [old_data[0] + [tim]] + rows
        else:
            data = [['Case', tim]] + [[src, fc] for src, fc in sts]

        write_csv(results_file, data)

    print(f'{dt:.3} secs elapsed, {dt / len(cases):.3} secs per case')


if __name__ == '__main__':
    main()

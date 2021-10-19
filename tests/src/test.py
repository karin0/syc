import time
import threading
import collections
import concurrent.futures as futures
from argparse import ArgumentParser
from datetime import datetime

from env import *
from cases import *

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


rid_cnt = collections.Counter()


def get_rid(case):
    k = case_iden(case[0])
    try:
        from pathvalidate import sanitize_filename
        k = sanitize_filename(k)
    except ImportError:
        pass

    if c := rid_cnt[k]:
        rid_cnt[k] = c + 1
        return f'{k}_{c}'
    rid_cnt[k] = 1
    return k


def main(ns):
    fail = ns.f
    desc = ns.m
    spec = ns.t
    no_stat = ns.n or fail or spec

    print(cpu_count, 'cores found')
    build()

    if fail:
        cases = get_fail_cases()
    elif spec:
        cases = resolve_cases(spec)
    else:
        cases = get_root_cases()

    mkdir('out')

    with futures.ThreadPoolExecutor(max_workers=cpu_count) as executor:
        dt = time.time()
        for fut in futures.as_completed([
            executor.submit(run, get_rid(case), *case)
            for case in cases
            # for i, case in enumerate(cases)
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

        st_sum = sum(float(st[1]) for st in sts)
        n = len(sts)
        sts.append(('~sum', '%.1f' % st_sum))
        sts.append(('~avg', '%.3f' % (st_sum / n)))
        if desc:
            sts.append(('~~desc', desc))

        if path.isfile(results_file):
            shutil.copy2(results_file, results_file + '.old.csv')
            old_data = read_csv(results_file)
            # TODO: this logic needs fixing, when some row has the last column empty
            # TODO: render diff and sum
            row_map = {}
            head = old_data[0]
            ow = len(old_data[0])
            if head[-1] == 'delta':
                head.pop()
                ow -= 1
            for row in old_data[1:]:
                if len(row) < ow:
                    row += [''] * (ow - len(row))
                elif len(row) > ow:
                    assert len(row) == ow + 1
                    row.pop()
                src = row[0]
                row_map[src] = row
            has_diff = False
            for src, fc in sts:
                row = row_map.get(src)
                if row is None:
                    row_map[src] = [src] + [''] * (ow - 1) + [fc]
                else:
                    row.append(fc)
                    if src != '~~desc':
                        ofc = row[-2]
                        if ofc:
                            has_diff = True
                            row.append(str(float(fc) - float(ofc)))
            rows = list(row_map.values())
            rows.sort(key=key0)
            head.append(tim)
            if has_diff:
                head.append('delta')
            data = [head] + rows
        else:
            data = [['Case', tim]] + [[src, fc] for src, fc in sts]

        write_csv(results_file, data)

    print(f'{dt:.3} secs elapsed, {dt / len(cases):.3} secs per case')


if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument('-f', action='store_true')
    parser.add_argument('-n', action='store_true')
    parser.add_argument('-t')
    parser.add_argument('-m')
    parser.parse_args()
    main(parser.parse_args())

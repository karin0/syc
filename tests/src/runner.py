import shutil
from conf import *


def read_course_stat(fn: str) -> str:
    res = []
    with open(fn, encoding='utf-8') as fp:
        for line in fp:
            p = line.find(':')
            if p >= 0:
                l = line[:p].strip()
                r = line[p + 1:].strip()
                res.append(l)
                res.append(r)
    res.append('cnt 0')
    print('course res', res)
    return ' '.join(res)


def err_runner(wd, src_file, in_file, ans_file, msg):
    join = lambda fn: path.join(wd, fn)
    parsed_ans_file = join('ans.txt')
    out_file = join('out.txt')

    with open(ans_file, encoding='utf-8') as inf:
        a = [s.split() for s in inf.read().splitlines()]
        for x in a:
            x[0] = int(x[0])
        a.sort()
        with open(parsed_ans_file, 'w', encoding='utf-8') as outf:
            for p in a:
                print(*p, file=outf)

    with open(out_file, 'wb') as fp:
        subprocess.run(
            [syc_path, path.realpath(src_file)],
            timeout=compile_timeout,
            check=True,
            cwd=wd,
            stdout=fp
        )

    if subprocess.run(diff + [parsed_ans_file, out_file],
                      stdout=subprocess.DEVNULL,
                      stderr=subprocess.DEVNULL).returncode:
        msg('differs')
    else:
        msg('ok')


def std_runner(rid, src_file, in_file, ans_file, msg):
    asm_fn = path.join(rid, 'out.asm')
    in_fn = path.join(rid, 'in.txt')
    out_fn = path.join(rid, 'out.txt')
    gcc_src_fn = path.join(rid, 'src.c')
    gcc_out_fn = path.join(rid, 'a.out')

    if not in_file:
        msg('no input file')
        in_file = os.devnull

    with open(in_file, encoding='utf-8') as fp:
        with open(in_fn, 'w', encoding='utf-8') as in_fp:
            for line in fp:
                for x in line.split():
                    in_fp.write(x)
                    in_fp.write('\n')

    msg('compiling')
    subprocess.run(
        [syc_path, path.realpath(src_file), '-o', path.realpath(asm_fn)],
        timeout=compile_timeout,
        check=True,
        cwd=rid
    )

    with open(in_fn, 'rb') as in_fp:
        if not ans_file:
            ans_file = path.join(rid, 'ans.txt')
            shutil.copy(header_path, gcc_src_fn)
            with open(gcc_src_fn, 'ab') as src_fp:
                with open(src_file, 'rb') as fp:
                    src_fp.write(fp.read())

            call([gcc, '-O0', '-x', 'c', '-o', gcc_out_fn, gcc_src_fn])
            with open(ans_file, 'wb') as ans_fp:
                subprocess.run(
                    [gcc_out_fn],
                    stdin=in_fp,
                    stdout=ans_fp,
                    check=True
                )

        with open(out_fn, 'wb') as out_fp:
            msg('simulating')
            in_fp.seek(0)
            stat = subprocess.run(
                ['java', '-jar', mars_path, 'nc', 'me', 'mc', 'Default', path.realpath(asm_fn)],
                stdin=in_fp,
                stdout=out_fp,
                stderr=subprocess.PIPE,
                check=True,
                timeout=mars_timeout,
                cwd=rid
            ).stderr
            if isinstance(stat, bytes):
                stat = stat.decode('utf-8')
            stat = stat.lower().strip()
            if stat.startswith('div'):
                msg(stat)
                res = (src_file, stat)
            elif stat:
                raise RuntimeError('mars complained: ' + stat)
            else:
                stat = read_course_stat(path.join(rid, 'InstructionStatistics.txt'))
                msg('course stat ' + stat)
                res = (src_file, stat)

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
    return res

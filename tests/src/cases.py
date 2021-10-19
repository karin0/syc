from conf import path, os, original_cwd

cases_root = '/home/karin0/lark/buaa/ct/cases'
root_926 = '/home/karin0/lark/buaa/ct/cases/926/testfiles'


def make_course_case(dir, i):
    return (path.join(dir, f'testfile{i}.txt'),
            path.join(dir, f'input{i}.txt'),
            None)


def get_the_cases():
    return [(path.join(cases_root, 'loop.c'), path.join(cases_root, '1.in'), None)]


def resolve_cases(s):
    if len(s) >= 3 and s[0] in 'ABC' and s[1] == '-':
        x = s[2:]
        try:
            x = int(x)
        except ValueError:
            pass
        else:
            if x > 0:
                return [make_course_case(path.join(root_926, s[0]), x)]

    s = path.join(original_cwd, s)
    if path.isdir(s):
        src = path.join(s, 'case.txt')
        if path.isfile(src):
            inf = path.join(s, 'in.txt')
            return [(src, inf if path.isfile(inf) else None, None)]
        return find_cases(s)

    return []


def case_iden(s):
    s = path.relpath(s, cases_root)
    a = path.normpath(s).split(os.sep)
    if len(a) >= 2 and a[-1] == 'case.txt':
        return a[-2]
    if len(a) >= 3 and a[-3] == 'out':
        return 'last_' + a[-2]
    if 'fail-out' in s:
        return 'failure'
    if '926' in s:
        return a[-2] + '-' + a[-1][8:-4]
    if 'hw' in s:
        return 'hw-' + a[-1][8:-4]
    return s


def get_err_cases():
    return tuple((f'err/testfile{i}.txt', None, f'err/output{i}.txt') for i in range(1, 5))


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


def get_fail_cases():
    return [('fail-out/case.txt', 'fail-out/in.txt', None)]


def get_root_cases():
    return find_cases(cases_root)

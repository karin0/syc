import os
path = os.path

cases_root = '/home/karin0/lark/buaa/ct/cases'
a_dir = '/home/karin0/lark/buaa/ct/cases/926/testfiles/A'


def make_course_case(dir, i):
    return (path.join(dir, f'testfile{i}.txt'),
            path.join(dir, f'input{i}.txt'),
            None)


def get_the_cases():
    return [make_course_case(a_dir, '13')]


def case_iden(s):
    s = path.relpath(s, cases_root)
    a = path.normpath(s).split(os.sep)
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

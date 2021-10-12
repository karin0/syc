import os
import subprocess
import multiprocessing

path = os.path

project_dir = '..'
mars_path = '../../mars.jar'
gcc_path = 'gcc'

syc_path = 'build/syc'
project_dir = path.realpath(project_dir)
cpu_count = multiprocessing.cpu_count()


def call(args):
    subprocess.run(args).check_returncode()


def call_sh(s):
    if os.system(s) != 0:
        raise RuntimeError


def build():
    if not path.isdir('build'):
        os.mkdir('build')
    os.chdir('build')
    call(['cmake', project_dir, '-DCMAKE_BUILD_TYPE=Release'])
    call(['make', f'-j{cpu_count}'])
    os.chdir('..')


run_id_last = 0


def run(src_file, in_file, ans_file):
    global run_id_last
    rid = run_id_last
    run_id_last += 1

    print('testing', src_file)
    call([syc_path, src_file, '-o', 'out.asm'])

    if in_file:
        with open(in_file, encoding='utf-8') as fp:
            with open('in.txt', 'w', encoding='utf-8') as in_fp:
                for line in fp:
                    for x in line.split():
                        in_fp.write(x)
                        in_fp.write('\n')
        with open(in_file, 'rb') as in_fp:
            with open('out.txt', 'wb') as out_fp:
                subprocess.run(
                    ['java', '-jar', mars_path, 'nc', 'me', 'mc', 'Default', 'out.asm'],
                    stdin=in_fp,
                    stdout=out_fp
                ).check_returncode()

            if not ans_file:
                with open('ans.txt', 'wb') as ans_fp:
                    subprocess.run(
                        [gcc_path, '-O0', '-x', 'c', '-o', 'a.out', src_file],
                        stdin=in_fp,
                        stdout=ans_fp
                    ).check_returncode()

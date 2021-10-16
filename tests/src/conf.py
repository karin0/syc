import os
from util import path

cases_root = '/home/karin0/lark/buaa/ct/cases'
project_dir = '/home/karin0/lark/buaa/ct/syc'
mars_path = '/home/karin0/lark/buaa/ct/mars.jar'
results_file = 'results.csv'

compile_timeout = 5
mars_timeout = 8

diff = ['fc'] if os.name == 'nt' else ['diff', '-b']
cmake = 'cmake'
make = 'make'
gcc = 'gcc'

syc_path = path.realpath('build/syc')

file_dir = path.dirname(path.realpath(__file__))

header_path = path.join(file_dir, 'common.h')

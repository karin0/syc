import multiprocessing
from util import *

project_dir = '/home/karin0/lark/buaa/ct/syc'
# mars_path = '/home/karin0/lark/buaa/ct/mars.jar'
mars_path = '/home/karin0/lark/buaa/ct/Mars-Compile-2021.jar'  # must be absolute
results_file = 'results.csv'

compile_timeout = 5
mars_timeout = 8

diff = ['fc'] if os.name == 'nt' else ['diff', '-b']
cmake = 'cmake'
make = 'make'
gcc = 'gcc'

file_dir = path.dirname(path.realpath(__file__))
header_path = path.join(file_dir, 'common.h')

original_cwd = os.getcwd()
os.chdir(path.join(file_dir, '..'))
syc_path = path.realpath('build/syc')

cpu_count = multiprocessing.cpu_count()

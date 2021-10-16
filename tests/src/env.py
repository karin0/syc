import multiprocessing
from cases import *
from runner import *

get_cases = get_root_cases
default_runner = std_runner

cpu_count = multiprocessing.cpu_count()

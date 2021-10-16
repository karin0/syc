import os
import subprocess
import csv

path = os.path


def call(args):
    subprocess.run(args, check=True)


def mkdir(s):
    if not path.isdir(s):
        os.mkdir(s)


def write_csv(fn, rows):
    with open(fn, 'w', encoding='utf-8', newline='') as fp:
        w = csv.writer(fp)
        for row in rows:
            w.writerow(row)


def read_csv(fn):
    with open(fn, encoding='utf-8', newline='') as fp:
        return list(csv.reader(fp))

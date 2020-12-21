import os
from os import listdir
from os.path import isfile, join

mypath = os.path.dirname(os.path.realpath(__file__))
files = [f for f in listdir(mypath) if isfile(join(mypath, f))]
tfiles = []

for file in files:
    if '.csv' in file:
        tfiles.append(file)

files = tfiles

for file in files:
    print(file + ":")
    f = open(file, 'r')
    data = f.readline()

    avg = 0
    lst = data.split(' ')

    for idx, val in enumerate(lst):
        avg = avg + float(val)
    avg = avg/len(lst)
    print(avg)
    avg = 0


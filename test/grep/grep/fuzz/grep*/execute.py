import sys
import os
import subprocess

outdir = "out_noguide/queue/"

fn = sys.argv[1]

listf = open(fn, "r")
cnt = 0
for line in listf.readlines():
	print(cnt)
	cnt = cnt + 1
	fn2 = outdir + line.strip()
	f = open(fn2, 'r')
	l = f.readline().strip()
	cmd = "./grep-gcov " + l.replace('\0', ' ')
	print(cmd)
	try:
		subprocess.run(cmd.split(' '),  timeout=10)	
	finally:
		f.close()

listf.close()

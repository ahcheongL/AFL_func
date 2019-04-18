import sys
import os
fname = sys.argv[1]
pwd = os.getcwd()

f = open(fname, 'r')
tcindex = 0
fname = ""

for dirname in ["/grep1","/grep*","/grepNon"]:
	if not os.path.exists(pwd + dirname):
		os.makedirs(pwd+dirname
						)
for line in f.readlines():
	line = line[3:len(line)-2]
	if "grep1.dat" in line:
		fname = pwd + "/grep1/id:" + str(tcindex)
	elif "grep*.dat" in line:
		fname = pwd + "/grep*/id:" + str(tcindex)
	else:
		fname = pwd + "/grepNon/id:" + str(tcindex)
	o = open(fname, 'w')
	o.write(line)
	tcindex = tcindex + 1


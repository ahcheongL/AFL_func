import sys
fn = sys.argv[1]
f = open(fn, 'r')
fid = 0;
for line in f.readlines():
	fn2 = "inputs/input" + str(fid)
	f2 = open(fn2, 'w')
	f2.write(line.strip())
	f2.close()
	fid += 1
f.close()

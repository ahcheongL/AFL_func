import sys

fname = sys.argv[1]
f = open(fname, 'r');
o = open(".".join(fname.split('.')[:len(fname.split('.'))-1]) + ".csv", "w");

digits = ['0','1','2','3','4','5','6','7','8','9'];
cnt = 0;
for line in f.readlines():
		if line.strip() in "testcase score":
			break
		if cnt > 4000:
			break
		o.write(line.split(", ")[2])

f.close();
o.close()

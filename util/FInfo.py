import sys
NODETHRES = 0

fname = sys.argv[1]
f = open(fname, 'r');
o = open(".".join(fname.split('.')[:len(fname.split('.'))-1]) + "-pro.txt", "w");

funcline = 1
numfunc = 0
numfunc2 = 0
digits = ['0','1','2','3','4','5','6','7','8','9'];
for line in f.readlines():
	if funcline:
		if line[0] in digits:
			numfunc += int(line.strip())
		else:
			numnode = int(line.strip().split(' ')[1])
			funcline = 0
	else:
		numnode = numnode - 1
		if (numnode == 0):
			funcline = 1

o.write(str(numfunc) + "\n")
f.close()
f = open(fname, 'r')
funcline = 1
removeline = False
for line in f.readlines():
	if funcline:
		if line[0] in digits:
			continue
		else:
			numnode = int(line.strip().split(' ')[1])
			if numnode < NODETHRES:
				removeline = True
			else:
				removeline = False
				o.write(line)
				numfunc2 = numfunc2 + 1
			funcline = 0
	else:
		if not removeline:
			o.write(line)
		numnode = numnode - 1
		if (numnode == 0):
			funcline = 1

print(numfunc2)
f.close();
o.close()

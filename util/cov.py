import sys

fn = sys.argv[1]
f = open(fn, 'r');
o = open(fn+"add.txt" , 'w')

nOE = 0
nOL = 0
for line in f.readlines():
	if "Lines" in line:
		p = line.split(' ')[1][9:]
		p = float(p[:len(p)-1])
		n = int(line.strip().split(' ')[3])
		nOL = nOL + n
		nOE = nOE + p* n* 0.01

print(nOL)
print(nOE)

f.close()
o.close()

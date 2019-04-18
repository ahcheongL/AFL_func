import sys
import math

fn1 = sys.argv[1]
fn2 = sys.argv[2]
class funcpair:
	def __init__(self, targ, sec, rel):
		self.targ = targ
		self.sec = sec
		self.rel = rel

fps1 = []
fps2 = []

f1 = open(fn1,'r')
f1.readline()
for line in f1.readlines():
	line = line.strip().split(', ')
	if len(line) >= 4 and "nan" not in line[2]:
		fp = funcpair(line[0],line[1],float(line[2]))
		fps1.append(fp)

f1.close()
f2 = open(fn2, 'r')
f2.readline()
for line in f2.readlines():
	line = line.strip().split(', ')
	if len(line) >= 4 and "nan" not in line[2]:
		fp = funcpair(line[0],line[1],float(line[2]))
		fps2.append(fp)

f2.close()

diffsum = 0
match = 0
for fp1 in fps1:
	for fp2 in fps2:
		if fp1.targ == fp2.targ and fp1.sec == fp2.sec:
			match = match + 1
			diffsum = diffsum + math.sqrt(math.pow(fp1.rel - fp2.rel, 2))
			break

if match == 0:
	print("no match!")
else:
	print("diff : " + str(diffsum / match))

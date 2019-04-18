import sys

fname = sys.argv[1]
f = open(fname, 'r');
o = open(".".join(fname.split('.')[:len(fname.split('.'))-1]) + ".csv", "w");

digits = ['0','1','2','3','4','5','6','7','8','9'];
start = 0;
srcs = {};
cnt = 0;
for line in f.readlines():
		if start == 0:
			if line.strip() not in "testcase score":
				continue
			else:
				start = 1;
				continue
		tcs = line.split("out_noinit_catdoc2/queue/id:")[2:];
		for tc in tcs:
			if (cnt > 300):
				break
			src = tc.split(',')[1][4:]
			score = tc.split(', ')[1].strip()
			if src in srcs:
				srcs[src] = (srcs[src][0] + 1, srcs[src][1])
			else:
				srcs[src] = (1, score)
			cnt = cnt + 1;

for key in srcs:
	o.write(key + "," + str(srcs[key][0])+ "," + str(srcs[key][1])+ "\n");

f.close();
o.close()

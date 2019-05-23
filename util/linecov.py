import sys
import subprocess


if (len(sys.argv) < 4):
	print("python3 linecov.py list.txt out_dirname out_file.csv")
	exit()
 
listfn = sys.argv[1]
dicname = sys.argv[2]
outname = sys.argv[3]
f = open(listfn, 'r')

of = open(outname, 'w')
of.write("time,line_cov,branch_cov\n")

efn = "execute.sh"

rec_time_m = 0
prev_rec_time = 0
cov_line = ",,\n"
prev_linen = 0
fuzz_start = False
cmd = []
linen = 0

def executeTC(tcn):
	global prev_linen, prev_rec_time, cov_line
	print("Executing with time : " + str(rec_time_m) + ", linen : " + str(tcn))
	ef = open(efn, 'w')
	f2 = open(listfn,'r')
	lc = 0
	for line in f2.readlines():
		if lc == tcn:
			break
		if lc >= prev_linen:
			ef.write("timeout 0.05 ./grep-gcov " + dicname + line.strip() + " &> /dev/null" + "\n")
		lc = lc + 1
	prev_linen = tcn
	f2.close()
	ef.close()
	cmd = ["chmod", "+x", "execute.sh"]
	subprocess.run(cmd)
	cmd = ["bash", "./execute.sh"]
	subprocess.run(cmd)
	cmd = ["gcov" ,"-b", "grep-gcov.c"]
	try:
		res = subprocess.check_output(cmd)
	except subprocess.CalledProcessError as e:
		res = e.output
	linecov = res.decode("utf-8").split('\n')[1].split(' ')[1].replace("executed:","")
	branchcov = res.decode("utf-8").split('\n')[3].split(' ')[3].replace("once:", "")
	while (prev_rec_time < rec_time_m):
		of.write(str(prev_rec_time) + cov_line)
		prev_rec_time += 600
	prev_rec_time = rec_time_m + 600
	cov_line = "," + linecov + "," + branchcov + "\n"
	of.write(str(rec_time_m) + cov_line)


for line in f.readlines():
	if "time" in line:
		new_time_m = line.strip().split("time:")[-1]
		if "+cov" in new_time_m:
			new_time_m = int(new_time_m.replace(",+cov", ""))
		else:
			new_time_m = int(new_time_m)
		if (new_time_m >= rec_time_m):
			rec_time_m = int(new_time_m / 600) * 600
			executeTC(linen)
			rec_time_m = rec_time_m + 600	
	linen = linen + 1
executeTC(linen)

of.close()
f.close()

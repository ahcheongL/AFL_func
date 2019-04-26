import sys
import subprocess

listfn = sys.argv[1]
dicname = sys.argv[2]
outname = sys.argv[3]
f = open(listfn, 'r')

of = open(outname, 'w')
of.write("time,line_cov,total_line\n")

efn = "execute.sh"

rec_time_m = 0
prev_linen = 0
fuzz_start = False
cmd = []
linen = 0

def executeTC(tcn):
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
	cmd = ["gcov" , "-f", "grep.c"]
	try:
		res = subprocess.check_output(cmd)
	except subprocess.CalledProcessError as e:
		res = e.output
	res = res.decode("utf-8")
	f3 = open("cov.txt", "w")
	f3.write(res)
	f3.close()
	cmd = ["python", "/home/cheong/AFL_func/util/cov.py", "cov.txt"]
	try:
		res = subprocess.check_output(cmd)
	except subprocess.CalledProcessError as e:
		res = e.output
	res = res.decode("utf-8")
	res = res[:len(res)-1]
	res = res.split("\n")
	of.write(str(rec_time_m) + "," + res[1] + ',' + res[0] + "\n")


for line in f.readlines():
	if "time" in line:
		new_time_m = line.strip().split("time:")[-1]
		if "+cov" in new_time_m:
			new_time_m = int(new_time_m.replace(",+cov", ""))
		else:
			new_time_m = int(new_time_m)
		if (new_time_m >= rec_time_m):
			executeTC(linen)
			rec_time_m = rec_time_m + 10	
	linen = linen + 1
executeTC(linen)

of.close()
f.close()

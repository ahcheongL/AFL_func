import sys
fn = sys.argv[1]
f = open(fn,'r')
line = f.readline().strip()
line = line.replace(' ', '\0')
f.close()
f = open(fn, 'w')
f.write(line)
f.close()

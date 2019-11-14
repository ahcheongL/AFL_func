
inp = open("md5all.txt", "r")

idx = 0
idx2 = 0
out = open("md5_" + str(idx) + ".txt", "w")
for l in inp:
  out.write(l)
  idx2 += 1
  if idx2 > 20:
    out.close()
    idx += 1
    idx2 = 0
    if idx > 5: 
      break
    out = open ("md5_" + str(idx) + ".txt", "w")

out.close()
inp.close()

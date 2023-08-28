import sys, os, re
import subprocess


print ("Zapocinjem staticku analizu: \n")
print("------------------------------------------------------------")
for root, dirs, files in os.walk("."):
	for filename in files:
		ext=os.path.splitext(filename)[1]
		if ext in [".cpp", ".hpp", ".h"]:
			subprocess.run(["cppcheck", "--std=c++11", "--language=c++",(os.path.join(root, filename))])
			print("-------------------------------------------------------")
print("Analiza je zavrsena")

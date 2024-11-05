#!/usr/bin/env python3
import sys
from pathlib import Path

in_name = sys.argv[1]
out_name = f"{in_name}.h"

in_data = Path(in_name).read_bytes()

with open(out_name, "w") as f:
	f.write(f"uint8_t g{in_name.split('/')[-1].split('.')[0].title()}Data[]={{")
	
	for i in range(len(in_data)):
		f.write(str(in_data[i]))
		if (i + 1 != len(in_data)):
			f.write(",")
	
	f.write("};")

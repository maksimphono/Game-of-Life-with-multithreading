#from numpy import array, random
from random import random
from os import path

width  = 4505
height = 1008
file_name = "4505x1008"
threshold = 0.85

rows = ["." * width]

for i in range(height - 2):
    rows.append(
        "." + "".join([".*"[random() > threshold] for i in range(width - 2)]) + "."
    )

rows.append("." * width)


with open(path.join("./input", file_name), "w") as file:
    print(width, height, file = file)
    print("\n".join(rows), file = file)
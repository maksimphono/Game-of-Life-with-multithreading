#from numpy import array, random
from random import random
from os import path

width = 10003
height = 18
file_name = "10003x18"
threshold = 0.7

rows = ["." * width]

for i in range(height - 2):
    rows.append(
        "." + "".join([".*"[random() > threshold] for i in range(width - 2)]) + "."
    )

rows.append("." * width)


with open(path.join("./input", file_name), "w") as file:
    print(width, height, file = file)
    print("\n".join(rows), file = file)
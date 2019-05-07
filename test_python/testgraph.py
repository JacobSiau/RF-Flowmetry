import matplotlib.pyplot as plot
import math

f = open("/home/pi/Documents/rpi_xc111/test_envelope_data/7_5_2019_11_28_34_UTC.txt")
filelines = f.readlines()
testlines = []
data = []

if __name__ == '__main__':
    testlines = filelines[10:630]
    # actual start: 99 mm
    # actual length: 400 mm
    # actual end: 499 mm
    # data length: 827
    for i in range(0, len(testlines)):
        amplitude = float(testlines[i][0:6])
        dist_mm = 99 + (i * 400/827)
        dist_in = dist_mm * 0.039370 
        data.append((dist_in, amplitude))
    for i in data:
        print(str(i))
    xpoints = []
    ypoints = []
    for d in data:
        xpoints.append(d[0])
        ypoints.append(d[1])
    plot.plot(xpoints, ypoints)
    plot.show()

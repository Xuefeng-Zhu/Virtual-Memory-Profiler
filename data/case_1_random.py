from bokeh.plotting import figure, output_file, show

if __name__ == '__main__':
    output_file("case1_random.html", title="Case 1 Random Access")

    f = open("profile1.data")
    data = f.readlines()[:-1]

    start = int(data[0].split()[0])
    total_time = 0
    total_fault = 0
    x = []
    y = []

    for item in data:
        tmp = item.split()
        time = int(tmp[0]) - start
        if time >= 0:
            x.append(total_time)
            total_time += 20
            total_fault += int(tmp[2])
            y.append(total_fault)
        else:
            break

    p = figure(title="Random Access for Two Process",
               x_axis_label='Time(milliseconds)',
               y_axis_label='Accumulated page fault count')
    p.line(x, y, line_width=2)
    show(p)
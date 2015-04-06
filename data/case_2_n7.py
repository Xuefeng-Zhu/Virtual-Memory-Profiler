from bokeh.plotting import figure, output_file, show

if __name__ == '__main__':
    output_file("case2_n7.html", title="Case 2 for 7 Process")

    f = open("profile_n_7.data")
    data = f.readlines()[:-1]

    start = int(data[0].split()[0])
    total_time = 0
    total_util = 0
    x = []
    y = []

    for item in data:
        tmp = item.split()
        time = int(tmp[0]) - start
        if time >= 0:
            x.append(total_time)
            total_time += 50
            total_util += int(tmp[3])
            y.append(total_util)
        else:
            break

    p = figure(title="Multiprogramming for n = 7",
               x_axis_label='Time(milliseconds)',
               y_axis_label='Total Utilization')
    p.line(x, y, line_width=2)
    show(p)
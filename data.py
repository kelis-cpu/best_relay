import matplotlib.pyplot as plt


def plot_data_from_file(file_path):
    # 用于存储每组数据的 x 和 y 值
    x_sets = []
    y_sets = []
    algos = []

    # 打开文件进行读取
    with open(file_path, 'r') as file:
        lines = file.readlines()
        # 每两行作为一组数据
        for i in range(0, len(lines), 3):
            algos.append(lines[i])
            # 解析 x 轴数据，以逗号分隔
            x = [float(num) for num in lines[i + 1].strip().split(',')]
            # 解析 y 轴数据，以逗号分隔
            y = [float(num) for num in lines[i + 2].strip().split(',')]
            # 将当前组的 x 和 y 数据添加到对应的列表中
            x_sets.append(x)
            y_sets.append(y)
    markers = ['o', 's', '^', 'D', 'v']  # 定义不同的标记样式
    colors = ['r', 'g', 'b', 'c', 'm']  # 定义不同的颜色

    # 绘制每条曲线
    for i in range(len(x_sets)):
        marker = markers[i % len(markers)]  # 循环使用标记样式
        color = colors[i % len(colors)]  # 循环使用颜色
        plt.plot(x_sets[i], y_sets[i], label=algos[i])
        # 为每个数据点添加标记
        plt.scatter(x_sets[i], y_sets[i], marker=marker, color=color, edgecolors='k')

    # 设置图形标题
    # plt.title('Multiple Curves from File')
    # 设置 x 轴标签
    plt.xlabel('Receive TX Node Ratio')
    # 设置 y 轴标签
    plt.ylabel('Latency(ms)')
    # 显示图例
    plt.legend()
    # 显示网格线
    plt.grid(True)
    # svg保存
    plt.savefig("diff_ratio_latency.svg")
    # 显示绘制好的图形
    plt.show()


# 调用函数并传入文件路径
file_path = 'data.txt'
plot_data_from_file(file_path)
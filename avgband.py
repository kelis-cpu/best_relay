import matplotlib.pyplot as plt

def plot_algorithm_latency(file_path):
    algorithm_names = []
    average_latencies = []

    try:
        # 打开文件
        with open(file_path, 'r') as file:
            # 逐行读取文件内容
            for line in file:
                # 去除行末的换行符并按逗号分割数据
                parts = line.strip().split(' ')
                if len(parts) == 2:
                    # 提取算法名称
                    algorithm_names.append(parts[0])
                    try:
                        # 提取平均延迟并转换为浮点数
                        average_latencies.append(int(parts[1]))
                    except ValueError:
                        print(f"无法将 '{parts[1]}' 转换为有效的数值。")
    except FileNotFoundError:
        print(f"文件 '{file_path}' 未找到。")
        return


    # 定义颜色列表
    colors = ['r', 'g', 'b', 'c', 'm', 'y', 'k']

    # 创建柱状图，并为每个柱子设置不同颜色
    bars = plt.bar(algorithm_names, average_latencies, color=[colors[i % len(colors)] for i in range(len(algorithm_names))])

    # 设置图表标题和坐标轴标签
    # plt.title(' of Different Algorithms')
    plt.xlabel('Algorithm Name')
    plt.ylabel('Duplicate INV msg numbers')

    # 添加数据标签
    for bar in bars:
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width() / 2, height, str(height), ha='center', va='bottom')

    # 显示图表
    plt.show()

# 请替换为你的实际文件路径
file_path = 'avgband.txt'
plot_algorithm_latency(file_path)
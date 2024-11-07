'''
Author: kelise
Date: 2024-11-03 23:11:02
LastEditors: kelis-cpu
LastEditTime: 2024-11-03 23:12:38
Description: file content
'''
import matplotlib.pyplot as plt
import numpy as np

# 数据
algorithms = ['best_relay', 'bitcoin']
delays = [2433, 2534]

# 设置柱状图的宽度
bar_width = 0.4
x = np.arange(len(algorithms))  # 每个算法的 x 位置

# 创建柱状图
bars = plt.bar(x, delays, width=bar_width, color=['blue', 'orange'])

# 在每个柱状图上方标注延迟值
for bar in bars:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2, yval, int(yval), ha='center', va='bottom')

# 设置 x 轴刻度
plt.xticks(x, algorithms)

# 添加标签和标题
plt.xlabel('Algorithms')
plt.ylabel('Averange Latency (ms)')

# 显示图形
plt.tight_layout()
plt.show()

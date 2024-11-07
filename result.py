import matplotlib.pyplot as plt
import numpy as np

# 数据
labels = ['0.8', '0.9', '1']
values = [
    [13149, 13158],  # 第一组数据
    [18793, 18955],  # 第二组数据
    [49661, 64000]   # 第三组数据
]

# 将数据转置为列，以便分别绘制
values = np.array(values)

# 设置柱状图的宽度
bar_width = 0.35
x = np.arange(len(labels))  # 每组数据的 x 位置

# 创建柱状图
bars1 = plt.bar(x - bar_width / 2, values[:, 0], width=bar_width, label='best_relay', color='blue')
bars2 = plt.bar(x + bar_width / 2, values[:, 1], width=bar_width, label='bitcoin', color = 'orange')

# 在每个柱状图上方标注数据
for bar in bars1:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2, yval, int(yval), ha='center', va='bottom')

for bar in bars2:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2, yval, int(yval), ha='center', va='bottom')

# 设置 x 轴刻度
plt.xticks(x, labels)

# 添加标签和标题
plt.xlabel('Received Node Ratio')
plt.ylabel('Total Txns')
plt.legend()

# 显示图形
plt.tight_layout()
plt.show()

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator

file_names = ['less_stepdata.dt', 'normal_stepdata.dt']
pdf_names = ['less', 'normal']

step_datas = []  # {step: probability density}

def read_data():
    for filename in file_names:
        step_data = {}
        with open(filename, 'r') as f:
            all_data = f.readlines()
            nodes = 0
            for line in all_data:
                try:
                    step = int(line)
                    nodes += 1
                    if step_data.get(step) is not None:
                        step_data[step] += 1
                    else:
                        step_data[step] = 1
                except ValueError:
                    print(line)
                    break
            # 计算概率密度（PDF）
            for k in step_data:
                step_data[k] = step_data[k] / nodes  # 直接使用概率值（无需累积）
        step_datas.append(step_data)

read_data()

# 设置坐标轴
ax = plt.gca()
x_major_locator = MultipleLocator(1)
ax.xaxis.set_major_locator(x_major_locator)

# 绘制PDF图
for i in range(len(step_datas)):
    x = []
    y = []
    sort_steps = sorted(step_datas[i])
    
    for step in sort_steps:
        x.append(step)
        y.append(step_datas[i][step])  # 直接使用概率密度值
    
    plt.plot(x, y, marker='o', label=pdf_names[i])

plt.xlim(1, 15)
plt.ylim(0, 1.0)  # 概率密度通常不超过1（除非有特殊需求）
plt.xlabel("hop")
plt.ylabel("​​Incremental Coverage per Hop")
plt.legend()
plt.savefig('pdf.png')
plt.show()
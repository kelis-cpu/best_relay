import numpy as np
import matplotlib.pyplot as plt

# 定义Sigmoid函数
def sigmoid(z):
    return 1 / (1 + np.exp(-z))

# 生成输入特征 x 的值，范围从 0 到 10，间距为 0.1
x = np.arange(0, 20, 0.1)

# 定义 theta_0 和 theta_1 的值，其中两个 theta_0 相同，一个不同，theta_1 均为负
theta_0_values = [3, 4, 4]
theta_1_values = [-0.75, -0.5, -0.75]

# 创建一个图形窗口
plt.figure(figsize=(8, 6))

# 遍历不同的 theta_0 和 theta_1 组合
for theta_0, theta_1 in zip(theta_0_values, theta_1_values):
    # 计算线性组合 z
    z = theta_0 + theta_1 * x
    # 计算Sigmoid函数值
    sigmoid_values = sigmoid(z)
    # 绘制曲线并添加图例标签
    label = fr'$\theta_0 = {theta_0}, \theta_1 = {theta_1}$'
    plt.plot(x, sigmoid_values, label=label)

# 设置图表标题
plt.title('Three Sigmoid Curves with Different Theta Values')
# 设置 x 轴标签
plt.xlabel('hop')
# 设置 y 轴标签
plt.ylabel('Sigmoid(x)')
# 显示网格线
plt.grid(True)
# 显示图例
plt.legend()
# 自动调整布局
plt.tight_layout()

# 显示图形
plt.show()
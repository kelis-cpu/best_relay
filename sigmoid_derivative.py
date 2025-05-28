import numpy as np
import matplotlib.pyplot as plt


# 定义Sigmoid函数
def sigmoid(z):
    return 1 / (1 + np.exp(-z))


# 定义Sigmoid函数的导数
def sigmoid_derivative(z, theta_1):
    sig = sigmoid(theta_1 * z)
    return theta_1 * sig * (1 - sig)


# 生成输入数据
z = np.linspace(-10, 10, 1000)

# 定义不同的theta_1值
theta_1_values = [0.5, 1, 2]

# 绘制不同theta_1下Sigmoid函数导数的图像
plt.figure(figsize=(8, 6))
for theta_1 in theta_1_values:
    derivative_values = sigmoid_derivative(z, theta_1)
    label = fr'$\theta_1 = {theta_1}$'
    plt.plot(z, derivative_values, label=label)

plt.title('Derivatives of Sigmoid Function with Different $\theta_1$')
plt.xlabel('z')
plt.ylabel('Sigmoid\'(z)')
plt.grid(True)
plt.legend()
plt.tight_layout()

# 显示图像
plt.show()
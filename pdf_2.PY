import matplotlib.pyplot as plt
import numpy as np
from matplotlib.pyplot import MultipleLocator

pdf_names = {"4" : "less", "128" : "normal"}
def read_file(filename):
    """读取文件数据，返回hop和inv数量的列表"""
    hops = []
    invs = []
    with open(filename, 'r') as f:
        for line in f:
            if line.strip() == '---end---':
                break
            parts = line.strip().split()
            if len(parts) == 2:
                hops.append(int(parts[0]))
                invs.append(int(parts[1]))
    return hops, invs

def plot_probability_density(file1, file2):
    """绘制两个文件的概率密度图"""
    # 读取数据
    hops1, invs1 = read_file(file1)
    hops2, invs2 = read_file(file2)
    
    # 计算概率密度
    total_invs1 = sum(invs1)
    prob1 = [inv/total_invs1 for inv in invs1]
    
    total_invs2 = sum(invs2)
    prob2 = [inv/total_invs2 for inv in invs2]
    
    # 绘制图形
    ax = plt.gca()
    x_major_locator = MultipleLocator(1)
    ax.xaxis.set_major_locator(x_major_locator)
    # plt.figure(figsize=(10, 6))
    
    # 确保hops是有序的
    sorted_indices1 = np.argsort(hops1)
    sorted_hops1 = np.array(hops1)[sorted_indices1]
    sorted_prob1 = np.array(prob1)[sorted_indices1]
    
    sorted_indices2 = np.argsort(hops2)
    sorted_hops2 = np.array(hops2)[sorted_indices2]
    sorted_prob2 = np.array(prob2)[sorted_indices2]
    
    plt.plot(sorted_hops1, sorted_prob1, marker='o', label=pdf_names[file1])
    plt.plot(sorted_hops2, sorted_prob2, marker='s', label=pdf_names[file2])
    
    plt.xlabel('hop')
    plt.ylabel('Duplicate INV ratio per hop')
    plt.xlim(1, 15)
    plt.legend()
    # plt.grid(True)
    plt.savefig('pdf_2.png')
    plt.show()

# 使用示例 - 替换为你的实际文件名
file1 = '4'  # 第一个文件名
file2 = '128'  # 第二个文件名
plot_probability_density(file1, file2)
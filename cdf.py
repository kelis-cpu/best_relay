'''
Author: kelise
Date: 2023-12-05 18:27:59
LastEditors: kelis-cpu
LastEditTime: 2024-02-07 01:48:57
Description: file content
'''
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator

file_names = ['less_stepdata.dt', 'normal_stepdata.dt']
cdf_names = ['less', 'normal']

step_datas = [] # {step:pre of recvmsg_nodecnts}

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
                    else :
                        step_data[step] = 1
                except ValueError:
                    print(line)
                    break
            for (k,v) in step_data.items():
                        step_data[k] = v / nodes # 统计概率
        step_datas.append(step_data)
        
# dupmsgs_file = ['random_flood', 'cluster']
dupmsgs_file = ['8', '20']
dup_cdf = ['less','cluster']
dupmsgs = []
def read_dupmsgs():
    for filename in dupmsgs_file:
        msg_data = {}
        total_cnts = 0
        with open(filename, 'r') as f:
            all_data = f.readlines()
            for line in all_data:
                if line == '---end---\n':
                    break
                s_m = line.split()
                print(s_m)
                step = int(s_m[0])
                msgcnts = int(s_m[1])
                total_cnts += msgcnts
                
                msg_data[step] = msgcnts
            for k,v in msg_data.items():
                msg_data[k] = v / total_cnts
            dupmsgs.append(msg_data)

# read_data('normal_stepdata.dt')
# read_data('less_stepdata.dt')

read_data()
# print(step_datas)



# # 设置x坐标轴刻度
ax = plt.gca()
x_major_locator=MultipleLocator(1)
ax.xaxis.set_major_locator(x_major_locator)

for i in range(len(step_datas)):
    x = []
    y = []
    sort_steps = sorted(step_datas[i])
    
    for step in sort_steps:
        x.append(step)
        y.append(step_datas[i][step])
    cdf = np.cumsum(y)
    
    plt.plot(x, cdf, marker='o', label=cdf_names[i])
## cdf_2 代码
######
# read_dupmsgs()
# print(dupmsgs)
# for i in range(len(dupmsgs)):
#     x = []
#     y = []
#     sort_steps = sorted(dupmsgs[i])
    
#     for step in sort_steps:
#         x.append(step)
#         y.append(dupmsgs[i][step])
#     cdf = np.cumsum(y)
#     if dup_cdf[i] == 'cluster':
#         plt.plot(x, cdf, marker='o', label='normal')
#     else:
#         plt.plot(x, cdf, marker='o', label=dup_cdf[i])
########

# # plt.plot(x,y, marker="o",label="PMF")
# plt.plot(x,cdf,marker="o",label="CDF")
# plt.axis('off')
plt.xlim(1,13)
plt.ylim(0,1.5)
plt.xlabel("hop")
plt.ylabel("node cover ratio")
# plt.ylabel("INV messages number ratio")
plt.legend()
plt.savefig('cdf.png')
plt.show()


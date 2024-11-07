/*
 * @Author: kelise
 * @Date: 2023-12-13 22:48:29
 * @LastEditors: kelis-cpu
 * @LastEditTime: 2023-12-13 23:03:10
 * @Description: file content
 */
#include <vector>
using namespace std;

void adjustP(vector<int> peers, vector<int> duplicate_peers = {}, int mtxns = 0) {

}
int a,b,mtxns,duplicate_invs,tx_rate, n;
int pi = 0;

// 每个节点中继优先级调整公式
pi = a * mtxns + b * (duplicate_invs / tx_rate); // a,b为常系数, a >> b, tx_rate为区块链吞吐量

n = F(step); // 中继交易节点数量
relay list L // 中继交易节点
begin
if n == peers
   L = peers
   Relay(L)
else 
   sort(peers, p) // 根据节点中继优先级p排序
   L = chose(n) // 选择优先级靠前的n个节点
   duplicate_peers = Relay(L) // 返回发送无用INV消息的对等点
   adjustP(duplicate_peers) // 调整节点中继优先级
end

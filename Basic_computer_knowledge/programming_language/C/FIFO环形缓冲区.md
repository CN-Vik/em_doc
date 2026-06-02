# 一、C语言环形队列（FIFO循环缓冲区）
存放**小猫体重+ID**，和上面小猫链表配套使用，常用于缓存接收小猫上报数据。
## 1. 完整代码
```c
#include <stdio.h>
#include <stdlib.h>

// 和小猫结构体共用
typedef struct {
    int id;
    float weight;
} CatData;

#define QUEUE_MAX_LEN 8    // 队列最大容量，空出1位区分满/空
typedef struct {
    CatData buf[QUEUE_MAX_LEN];
    int front;   // 队头：出队下标
    int rear;    // 队尾：入队下标
} RingQueue;

// API声明
void RQ_Init(RingQueue *q);
int RQ_IsEmpty(RingQueue *q);
int RQ_IsFull(RingQueue *q);
int RQ_EnQueue(RingQueue *q, CatData data); //入队
int RQ_DeQueue(RingQueue *q, CatData *out); //出队
int RQ_GetCount(RingQueue *q);              //有效数据个数
void RQ_PrintAll(RingQueue *q);              //遍历队列

//初始化
void RQ_Init(RingQueue *q)
{
    q->front = q->rear = 0;
}

//判空
int RQ_IsEmpty(RingQueue *q)
{
    return q->front == q->rear;
}

//判满：牺牲一个存储空间
int RQ_IsFull(RingQueue *q)
{
    return (q->rear + 1) % QUEUE_MAX_LEN == q->front;
}

//入队：1成功 0满了失败
int RQ_EnQueue(RingQueue *q, CatData data)
{
    if(RQ_IsFull(q)) return 0;
    q->buf[q->rear] = data;
    q->rear = (q->rear + 1) % QUEUE_MAX_LEN;
    return 1;
}

//出队：1成功 0空
int RQ_DeQueue(RingQueue *q, CatData *out)
{
    if(RQ_IsEmpty(q)) return 0;
    *out = q->buf[q->front];
    q->front = (q->front + 1) % QUEUE_MAX_LEN;
    return 1;
}

//当前存储数据数量
int RQ_GetCount(RingQueue *q)
{
    return (q->rear - q->front + QUEUE_MAX_LEN) % QUEUE_MAX_LEN;
}

//遍历打印
void RQ_PrintAll(RingQueue *q)
{
    if(RQ_IsEmpty(q)){
        printf("队列为空\n");
        return;
    }
    int idx = q->front;
    int cnt = RQ_GetCount(q);
    printf("队列现有%d只小猫:\n",cnt);
    for(int i=0; i<cnt; i++)
    {
        printf("ID:%d,体重:%.2fkg\n",q->buf[idx].id,q->buf[idx].weight);
        idx = (idx+1)%QUEUE_MAX_LEN;
    }
}

//使用示例main
int main(void)
{
    RingQueue q;
    RQ_Init(&q);
    CatData tmp;

    //1.入队3只小猫
    tmp.id=101; tmp.weight=2.3f; RQ_EnQueue(&q,tmp);
    tmp.id=102; tmp.weight=3.1f; RQ_EnQueue(&q,tmp);
    tmp.id=103; tmp.weight=2.8f; RQ_EnQueue(&q,tmp);
    RQ_PrintAll(&q);

    //2.出队一只
    RQ_DeQueue(&q,&tmp);
    printf("出队小猫：ID=%d,体重=%.2f\n",tmp.id,tmp.weight);
    RQ_PrintAll(&q);

    //3.查看数量
    printf("剩余数量：%d\n",RQ_GetCount(&q));

    return 0;
}
```

## 2. 怎么使用（极简使用步骤）
```c
//1.定义队列变量
RingQueue cat_queue;
//2.初始化
RQ_Init(&cat_queue);

//3.存数据(入队)
CatData cat1={1,2.5f};
RQ_EnQueue(&cat_queue,cat1);

//4.取数据(出队)
CatData get;
if(RQ_DeQueue(&cat_queue,&get))
{
    //读取get.id/get.weight
}

//5.辅助接口
RQ_GetCount(&cat_queue); //当前多少数据
RQ_IsFull(&cat_queue);   //是否塞满
RQ_IsEmpty(&cat_queue);  //是否空
RQ_PrintAll(&cat_queue); //全部打印
```

# 二、环形队列 vs 之前链表（选用场景）
|结构|适用场景|
|----|--------|
|**单链表**|长期存储、频繁随机查找/增删改小猫信息（档案库）|
|**环形队列**|临时缓存、先进先出、串口/中断实时接收数据（数据流缓冲区）|

# 三、环形队列应用前景（嵌入式最常用）
## 1. 单片机/嵌入式（最主流）
1. **串口收发缓存**：UART中断收到数据丢进环形队列，主循环慢慢解析，防止丢字节（蓝牙、RS485、猫砂盆传感器数据）
2. **ADC采样缓存**：定时采集传感器数据压入队列，后台批量处理滤波
3. **按键/消息缓冲区**：RTOS任务间消息传递，FreeRTOS消息队列底层就是环形队列

## 2. PC软件、网络编程
1. TCP接收缓冲区：网卡源源不断收数据，存入环形缓冲，应用层慢慢读取解析
2. 音视频缓存：音频PCM、视频帧环形缓冲，解决生产速率≠消费速率（生产者消费者模型）

## 3. 操作系统底层
- 内核设备驱动、管道pipe、socket缓冲区全是环形队列实现
- 磁盘IO块缓冲

# 四、核心特点总结
1. **固定内存**：数组实现，不用malloc/free，无内存碎片（嵌入式首选）
2. **FIFO先进先出**：先来数据先走，适合数据流
3. **头尾绕圈复用空间**，填满自动停止入队
4. **缺点：不能随机中间删除/插入**，所以不适合长期档案存储（用链表）

需要我：**环形队列+小猫链表组合示例**（传感器数据进队列，满了存入链表归档）吗？
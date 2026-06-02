#include <stdio.h>
#include <stdlib.h>

// 1. 定义小猫结构体
typedef struct Cat {
    int id;         // 小猫唯一ID
    float weight;   // 小猫体重
    struct Cat *next; // 链表指针，指向下一只小猫
} CatNode, *CatList;

// 2. 函数声明（所有API接口）
void initList(CatList *head);                // 初始化链表
int getCatCount(CatList head);               // 获取小猫总数
void showAllCats(CatList head);              // 遍历所有小猫
int addCat(CatList head, int id, float weight); // 增加小猫
int deleteCat(CatList head, int id);        // 删除小猫（按ID）
CatNode* findCat(CatList head, int id);     // 查找小猫（按ID）
int modifyCat(CatList head, int id, float newWeight); // 修改小猫信息
void freeList(CatList head);                // 释放链表内存

// 3. 主函数：菜单测试所有功能
int main() {
    CatList head;
    initList(&head); // 初始化链表
    int choice, id;
    float weight;

    while (1) {
        // 菜单
        printf("\n===== 小猫信息管理系统 =====\n");
        printf("1. 新增小猫\n");
        printf("2. 删除小猫（按ID）\n");
        printf("3. 查找小猫（按ID）\n");
        printf("4. 修改小猫体重\n");
        printf("5. 遍历所有小猫\n");
        printf("6. 查看小猫总数\n");
        printf("0. 退出系统\n");
        printf("请输入操作序号：");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("请输入小猫ID：");
                scanf("%d", &id);
                printf("请输入小猫体重(kg)：");
                scanf("%f", &weight);
                addCat(head, id, weight) ? printf("新增成功！\n") : printf("ID已存在，新增失败！\n");
                break;
            case 2:
                printf("请输入要删除的小猫ID：");
                scanf("%d", &id);
                deleteCat(head, id) ? printf("删除成功！\n") : printf("未找到该ID小猫！\n");
                break;
            case 3:
                printf("请输入要查找的小猫ID：");
                scanf("%d", &id);
                CatNode *cat = findCat(head, id);
                if (cat) printf("找到小猫：ID=%d，体重=%.2fkg\n", cat->id, cat->weight);
                else printf("未找到该小猫！\n");
                break;
            case 4:
                printf("请输入要修改的小猫ID：");
                scanf("%d", &id);
                printf("请输入新体重：");
                scanf("%f", &weight);
                modifyCat(head, id, weight) ? printf("修改成功！\n") : printf("未找到该ID小猫！\n");
                break;
            case 5:
                showAllCats(head);
                break;
            case 6:
                printf("当前小猫总数：%d 只\n", getCatCount(head));
                break;
            case 0:
                freeList(head); // 退出前释放内存
                printf("已释放内存，退出系统！\n");
                return 0;
            default:
                printf("输入错误，请重新输入！\n");
        }
    }
}

// 初始化链表（创建头节点）
void initList(CatList *head) {
    *head = (CatList)malloc(sizeof(CatNode));
    (*head)->next = NULL;
}

// API1：获取小猫总数量
int getCatCount(CatList head) {
    int count = 0;
    CatNode *p = head->next;
    while (p) {
        count++;
        p = p->next;
    }
    return count;
}

// API2：遍历所有小猫
void showAllCats(CatList head) {
    CatNode *p = head->next;
    if (!p) {
        printf("链表为空，没有小猫！\n");
        return;
    }
    printf("\n===== 所有小猫信息 =====\n");
    while (p) {
        printf("小猫ID：%d\t体重：%.2fkg\n", p->id, p->weight);
        p = p->next;
    }
}

// API3：增加小猫（尾部添加，ID唯一不重复）
int addCat(CatList head, int id, float weight) {
    // 检查ID是否重复
    if (findCat(head, id)) return 0;

    CatNode *newCat = (CatNode*)malloc(sizeof(CatNode));
    newCat->id = id;
    newCat->weight = weight;
    newCat->next = NULL;

    // 找到链表尾部
    CatNode *p = head;
    while (p->next) p = p->next;
    p->next = newCat;
    return 1;
}

// API4：删除小猫（按ID）
int deleteCat(CatList head, int id) {
    CatNode *p = head, *q;
    // 找到要删除节点的前驱节点
    while (p->next && p->next->id != id) {
        p = p->next;
    }
    // 未找到
    if (!p->next) return 0;

    // 删除节点
    q = p->next;
    p->next = q->next;
    free(q);
    return 1;
}

// API5：查找小猫（按ID，返回节点指针）
CatNode* findCat(CatList head, int id) {
    CatNode *p = head->next;
    while (p) {
        if (p->id == id) return p;
        p = p->next;
    }
    return NULL;
}

// API6：修改小猫信息（按ID修改体重）
int modifyCat(CatList head, int id, float newWeight) {
    CatNode *cat = findCat(head, id);
    if (!cat) return 0;
    cat->weight = newWeight;
    return 1;
}

// 释放整个链表内存
void freeList(CatList head) {
    CatNode *p = head, *q;
    while (p) {
        q = p->next;
        free(p);
        p = q;
    }
}
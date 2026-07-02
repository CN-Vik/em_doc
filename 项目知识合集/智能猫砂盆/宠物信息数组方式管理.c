#include <stdio.h>
#include <string.h>

// 猫咪数据结构
typedef struct {
    int id;         // 猫咪唯一ID
    int weight;     // 最新体重（单位：克）
} tCat;

#define CAT_MAX_NUM 10
tCat g_cat_info[CAT_MAX_NUM];
// 记录数组当前有效猫咪数量
int cat_count = 0;

/**
 * @brief 按下标删除数组中的猫咪
 * @param index 待删除元素下标
 * @return 0成功，-1下标非法
 */
int Cat_DelByIndex(tCat *cat_info,int index)
{
    // 下标合法性校验
    if (index < 0 || index >= cat_count)
    {
        printf("delte_fail:index_overstep!\n");
        return -1;
    }

    printf("del_cat_ok,index:%d,Cat_ID:%d,cat_weigh:%d \n",
        index,
        cat_info[index].id,
        cat_info[index].weight
    );

    // 后续元素全部向前移位覆盖待删除数据
    for (int i = index; i < cat_count - 1; i++)
    {
        cat_info[i] = cat_info[i + 1];
    }

    // 有效数量减一，清空最后一个无效位置（可选）
    memset(&cat_info[cat_count - 1], 0, sizeof(tCat));
    cat_count--;
    return 0;
}

/**
 * @brief 根据猫咪ID删除（推荐业务使用，ID唯一）
 * @param del_id 要删除的猫咪ID
 * @return 0成功，-1未找到该ID
 */
int Cat_DelById(tCat *cat_info, int del_id)
{
    int target_index = -1;
    // 遍历查找对应ID下标
    for (int i = 0; i < cat_count; i++)
    {
        if (cat_info[i].id == del_id)
        {
            target_index = i;
            break;
        }
    }

    if (target_index == -1)
    {
        printf("del_fail: not_found_catID=%d\n", del_id);
        return -1;
    }

    // 复用按下标删除逻辑
    return Cat_DelByIndex(cat_info,target_index);
}

// 辅助函数：新增猫咪，用于测试
int Cat_Add(tCat *cat_info,int id, int weight)
{
    if (cat_count >= CAT_MAX_NUM)
    {
        printf("array_is_full_canot_add_cat!\n");
        return -1;
    }
    cat_info[cat_count].id = id;
    cat_info[cat_count].weight = weight;
    cat_count++;
    return 0;
}

// 辅助函数：打印全部猫咪数据
void Cat_PrintAll(tCat *cat_info)
{
    printf("cat_list_had_num:%d\n", cat_count);
    for (int i = 0; i < cat_count; i++)
    {
        printf("index:%d | ID:%d | weigh:%dg\n",
               i, cat_info[i].id, cat_info[i].weight);
    }
    printf("================================\n\n");
}

int main(void)
{
    // 测试：添加测试数据
    Cat_Add(&g_cat_info, 0, 1000);
    Cat_Add(&g_cat_info, 1, 2000);
    Cat_Add(&g_cat_info, 2, 3000);
    Cat_Add(&g_cat_info, 3, 4000);
    Cat_Add(&g_cat_info, 4, 5000);
    Cat_PrintAll(&g_cat_info);

    // 1. 删除ID=1002的猫咪
    Cat_DelById(&g_cat_info,3);
    Cat_PrintAll(&g_cat_info);
    Cat_DelById(&g_cat_info,1);
    Cat_PrintAll(&g_cat_info);

    Cat_DelById(&g_cat_info,4);
    Cat_PrintAll(&g_cat_info);

    Cat_DelById(&g_cat_info,0);
    Cat_PrintAll(&g_cat_info);


    // // 2. 按下标删除第0号猫咪
    // Cat_DelByIndex(0);
    // Cat_PrintAll();

    // 3. 删除不存在的ID测试
    // Cat_DelById(9999);

    return 0;
}


/**
 
cat_list_had_num:5
index:0 | ID:0 | weigh:1000g
index:1 | ID:1 | weigh:2000g
index:2 | ID:2 | weigh:3000g
index:3 | ID:3 | weigh:4000g
index:4 | ID:4 | weigh:5000g
================================

del_cat_ok,index:3,Cat_ID:3,cat_weigh:4000
cat_list_had_num:4
index:0 | ID:0 | weigh:1000g
index:1 | ID:1 | weigh:2000g
index:2 | ID:2 | weigh:3000g
index:3 | ID:4 | weigh:5000g
================================

del_cat_ok,index:1,Cat_ID:1,cat_weigh:2000
cat_list_had_num:3
index:0 | ID:0 | weigh:1000g
index:1 | ID:2 | weigh:3000g
index:2 | ID:4 | weigh:5000g
================================

del_cat_ok,index:2,Cat_ID:4,cat_weigh:5000
cat_list_had_num:2
index:0 | ID:0 | weigh:1000g
index:1 | ID:2 | weigh:3000g
================================

del_cat_ok,index:0,Cat_ID:0,cat_weigh:1000
cat_list_had_num:1
index:0 | ID:2 | weigh:3000g
================================


--------------------------------
Process exited after 0.05524 seconds with return value 0
请按任意键继续. . .

 * 
 */
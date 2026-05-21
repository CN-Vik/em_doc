
# 有啥要求 把理想代码格式  和.clang-format 文件发给 AI 让AI 给修改


#  下边是案例（完美版本）



```bash
# .clang-format 配置文件 (兼容旧版本)

##
## 语言配置
##
Language: Cpp
Standard: Auto

##
## 基础格式设置
##
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 0

##
## 函数参数对齐设置 - 关键修改
##
AlignConsecutiveDeclarations: false # 禁用参数名列对齐
BinPackParameters: false # 禁止参数包装在一行
AllowAllParametersOfDeclarationOnNextLine: false # 强制每个参数独立一行

##
## 大括号风格配置
##
BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass: false
  AfterControlStatement: Always # 控制语句后总是换行
  AfterEnum: false
  AfterFunction: true # 函数定义后换行
  AfterNamespace: false
  AfterStruct: false
  AfterUnion: false
  AfterExternBlock: false
  BeforeCatch: false
  BeforeElse: false
  IndentBraces: false
  SplitEmptyFunction: false
  SplitEmptyRecord: false
  SplitEmptyNamespace: false
  AfterCaseLabel: true # case 标签后换行

##
## 宏和注释特殊处理
##
AlignConsecutiveMacros: true
AlignTrailingComments: true
SpacesBeforeTrailingComments: 2
SortIncludes: false

##
## 对齐和空格设置
##
AccessModifierOffset: -4
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: true
AlignEscapedNewlines: Right
AlignOperands: true
SpaceBeforeParens: ControlStatements
SpaceBeforeAssignmentOperators: true
SpaceAfterCStyleCast: false
SpaceAfterTemplateKeyword: true
SpaceInEmptyParentheses: false
SpacesInAngles: false
SpacesInCStyleCastParentheses: false
SpacesInContainerLiterals: true
SpacesInParentheses: false
SpacesInSquareBrackets: false

##
## 函数和代码块格式
##
AllowShortBlocksOnASingleLine: false # 禁止短代码块单行
AllowShortCaseLabelsOnASingleLine: false # case标签必须在多行
AllowShortFunctionsOnASingleLine: None # 函数不允许单行
AllowShortIfStatementsOnASingleLine: false # if语句必须多行
AllowShortLoopsOnASingleLine: false # 循环必须多行
AlwaysBreakAfterReturnType: None
AlwaysBreakBeforeMultilineStrings: false
AlwaysBreakTemplateDeclarations: true
BreakBeforeBinaryOperators: NonAssignment
BreakBeforeTernaryOperators: false
BreakConstructorInitializers: AfterColon
BreakInheritanceList: AfterColon
BreakStringLiterals: false
CompactNamespaces: true
ConstructorInitializerAllOnOneLineOrOnePerLine: false
ConstructorInitializerIndentWidth: 4
ContinuationIndentWidth: 4
Cpp11BracedListStyle: true
DerivePointerAlignment: false
FixNamespaceComments: true
IndentCaseLabels: true # case 标签缩进
IndentCaseBlocks: true # case 块缩进
IndentPPDirectives: BeforeHash
IndentWrappedFunctionNames: false
KeepEmptyLinesAtTheStartOfBlocks: true
MaxEmptyLinesToKeep: 2
NamespaceIndentation: None
PointerAlignment: Right
ReflowComments: true
SortUsingDeclarations: false

```



-----
-----

>代码修改完格式 如下所示

```c
static int aw2013_write(struct aw2013_led *led, u8 reg, u8 val)
{
    int ret = -1;
    unsigned char cnt = 0;

    switch (ret) 
    {
        case 0: 
        {
            break;
        }

        default: 
        {
            break;
        }
    }

    while (cnt < AW_I2C_RETRIES) 
    {
        ret = i2c_smbus_write_byte_data(led->client, reg, val);
        
        if (ret < 0) 
        {
            pr_err("%s: i2c_write error, cnt=%d, ret=%d\n", 
                   __func__, cnt, ret);
        } 
        else 
        {
            break;
        }

        cnt++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}
```

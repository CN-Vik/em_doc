# Vim 最简配置教程（新手直接抄）
## 1. 找到配置文件
**Linux/Mac/WSL**
```bash
vim ~/.vimrc
```
**Windows**
文件路径：`C:\Users\用户名\_vimrc`

## 2. 直接粘贴通用好用配置
```vim
" 基础设置
set number          " 显示行号
set relativenumber  " 相对行号
set cursorline      " 高亮当前行
set tabstop=4       " tab占4空格
set shiftwidth=4    " 缩进4空格
set expandtab       " tab转空格
set autoindent      " 自动缩进
set smartindent     " C语言智能缩进

set encoding=utf-8
set fileencodings=utf-8,gbk
set ignorecase      " 搜索忽略大小写
set smartcase
set hlsearch        " 高亮搜索结果
set incsearch       " 实时搜索

set mouse=a         " 开启鼠标
set clipboard=unnamed " 系统剪贴板互通

" 配色
syntax enable
set background=dark
colorscheme desert
```

## 3. 生效
保存退出 `:wq`
**重新打开 vim 立刻生效**

## 4. 常用快捷键
- `dd` 删除整行
- `yy` 复制整行
- `p` 粘贴
- `u` 撤销
- `Ctrl+r` 恢复
- `:w` 保存
- `:q` 退出
- `:wq` 保存退出
- `i` 进入编辑模式
- `Esc` 退出编辑

## 5. 写C语言必备（加进.vimrc）
```vim
" C语言自动补全+格式化
autocmd FileType c setlocal cindent
```

## 6. 一键安装插件管理器（vim-plug）
### 安装
```bash
curl -fLo ~/.vim/autoload/plug.vim --create-dirs \
https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim
```
### 装C代码高亮/补全
```vim
call plug#begin('~/.vim/plugged')
Plug 'preservim/nerdtree'    " 文件树
Plug 'vim-airline/vim-airline' " 状态栏
Plug 'octol/vim-cpp-enhanced-highlight' " C高亮
call plug#end()
```
打开vim输入：`:PlugInstall` 自动安装

需要我给你**嵌入式开发专用 Vim 配置**吗？

-------
-------
-----
# 嵌入式开发专用 Vim 完整配置
## 一、编辑配置文件
```bash
vim ~/.vimrc
```

## 二、全量嵌入式专属配置直接粘贴
```vim
" ========== 嵌入式开发 VIM 配置 ==========
" 基础界面
set number                  " 显示行号
set relativenumber           " 相对行号，跳转更方便
set cursorline               " 高亮当前行
set signcolumn=yes           " 固定符号列，代码不跳动
set laststatus=2             " 始终显示状态栏

" 缩进规范（嵌入式统一4空格）
set tabstop=4
set shiftwidth=4
set softtabstop=4
set expandtab                " Tab转为空格，工程统一格式
set autoindent
set smartindent
set cindent                  " C语言精准缩进

" 编码兼容（Linux/Windows交叉编译必备）
set encoding=utf-8
set fileencodings=utf-8,gb2312,gbk
set termencoding=utf-8

" 搜索优化
set ignorecase
set smartcase
set hlsearch
set incsearch
nnoremap <silent> <ESC> :nohlsearch<CR> " ESC清空搜索高亮

" 实用功能
set mouse=a                  " 开启鼠标点击拖动
set clipboard=unnamedplus    " 系统剪贴板互通，复制代码无缝切换
set scrolloff=5              " 上下保留5行视野
set nowrap                   " 代码不自动换行，看寄存器/宏更清晰
set backup=no                " 关闭备份文件
set swapfile=no              " 关闭交换文件

" 语法高亮
syntax enable
syntax on
set background=dark
colorscheme desert           " 护眼深色主题

" ========== 嵌入式语言专属配置 ==========
autocmd FileType c,h,inc setlocal expandtab shiftwidth=4 tabstop=4
autocmd FileType make setlocal noexpandtab shiftwidth=8 tabstop=8 " Makefile用原生Tab
autocmd FileType sh setlocal expandtab shiftwidth=2 tabstop=2

" 一键编译运行（Linux/WSL 编译C代码）
nnoremap <F5> :!gcc % -o %< -Wall -O2 -g<CR>
nnoremap <F6> :!./%<<CR>

" 快速保存退出
nnoremap <C-s> :w<CR>
inoremap <C-s> <ESC>:w<CR>a

" 快速注释 嵌入式高频用
nnoremap <C-/> :s/^/\/\//g<CR>
vnoremap <C-/> :s/^/\/\//g<CR>

" ========== 插件配置 ==========
call plug#begin('~/.vim/plugged')
Plug 'preservim/nerdtree'               " 左侧文件树 浏览工程源码
Plug 'vim-airline/vim-airline'          " 状态栏显示文件格式/编码
Plug 'octol/vim-cpp-enhanced-highlight' " C语言宏、寄存器精准高亮
Plug 'tpope/vim-commentary'             " 快捷批量注释
Plug 'preservim/tagbar'                 " 右侧函数/结构体大纲
call plug#end()

" 插件快捷键
nnoremap <F3> :NERDTreeToggle<CR>       " F3打开关闭文件树
nnoremap <F4> :TagbarToggle<CR>         " F4打开函数大纲
```

## 三、安装插件管理器
```bash
curl -fLo ~/.vim/autoload/plug.vim --create-dirs https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim
```

## 四、安装所有嵌入式插件
打开 `vim` 输入命令回车
```
:PlugInstall
```

## 五、嵌入式专属快捷键
|按键|功能|
|----|----|
|F3|打开工程文件树|
|F4|查看结构体/函数/宏大纲|
|F5|一键编译C代码（自动带CFLAGS警告）|
|F6|运行编译好的程序|
|Ctrl+/|批量单行注释|
|Ctrl+s|快速保存|
|ESC|清除搜索高亮|

## 六、适配场景
1. **STM32/ESP32 源码阅读**：结构体、宏定义、寄存器高亮清晰
2. **Makefile 编写**：自动切换Tab格式，编译不出错
3. **WSL/Linux 交叉编译**：编码兼容不乱码
4. **大量C工程翻阅**：文件树+函数大纲极速找代码

保存退出 `:wq` 重启vim直接生效！

需要我再给你配**VIM看内核源码、跳定义**的ctags配置吗？
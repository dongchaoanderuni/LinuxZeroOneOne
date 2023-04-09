#源码结构

linux-0.11/
├── boot/               # 引导加载程序目录
│   ├── bootsect.s      # 第一阶段启动扇区汇编代码
│   ├── setup.s         # 第二阶段启动设置处理器保护模式
│   └── head.s          # 开启分页机制，进入main函数处理
├── include/            # 内核头文件目录 
├── init/               # 系统初始化代码
│   ├── main.c          # 内核初始化及用户进程创建代码
│   └── Makefile        # init源代码make文件
├── kernel/             # 内核源代码 
│   ├── asm/            # CPU平台相关文件 
│   ├── fs/             # 文件系统代码
│   ├── mm/             # 内存管理代码 
│   ├── sched/          # 进程调度代码 
│   ├── sys/            # System Call接口代码   
│   └── Makefile        # 内核源代码make文件
├── lib/                # 基础库代码 
├── tools/              # 工具和实用程序
└── Makefile            # 整个内核Makefile  



Linux 0.11 Source Code Structure
根目录 (./)
Makefile：编译内核的 makefile 文件
README：有关内核运行和使用的信息
setup.s：启动代码
/Documentation
various : 各种文档和帮助文件
/include
*.h：标准头文件和系统特定头文件
/kernel
asm.s：定义了系统级函数，如保存CPU寄存器、打开/关闭中断等。这些函数通常是用于内核开发人员在C代码中使用的宏。
blk_drv：块设备驱动程序，如磁盘驱动程序。
chr_drv：字符设备驱动程序，如控制台和串口驱动程序。
math.i：浮点数库源码。
sched.c：调度程序负责管理进程。
sys_call.s：系统调用接口（API），为用户进程提供内核级功能。
/fs
inode.c: 索引节点(inode) 操作代码
buffer.c: 缓冲区缓存代码
file_dev.c: 设备文件代码
inode_dev.c: 块设备文件代码
namei.c: 名称查找代码
super.c: 超级块操作和安装代码
exec.c: 执行用户进程代码
link.c: 链接文件代码
pipe.c: 管道代码
/mm
mem.c：物理内存管理代码，如初始化物理内存、页面分配等。
page.s：内存分页编码如发生缺页时，调用的中断例程寻找适当的空间来放置新的页面。
swap.c：在交换文件中为进程提供虚拟内存，并处理闲置或休眠状态的页面读取和写入。
/init
main.c：main函数，初始化内核的各个组件。
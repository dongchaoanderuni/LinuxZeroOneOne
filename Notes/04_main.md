本文主要介绍main.c中的程序,主要分为：
- 1. 规划物理内存格局，设置缓冲区、虚拟盘、主内存
- 2. 设置虚拟盘空间并初始化
- 3. 内存管理结构mem_map初始化
- 4. 异常处理类中断服务程序挂接

![main_01](../images/04_main/main_01.png#pig_center)

Intel 本身对于访问内存就分成三类：

代码

数据

栈

而 Intel 也提供了三个段寄存器来分别对应着三类内存：

代码段寄存器（cs）
数据段寄存器（ds）
栈段寄存器（ss）

具体来说：

CS:EIP 表示了我们要执行哪里的代码。
DS:XXX 表示了我们要访问哪里的数据。
SS:ESP 表示了我们的栈顶地址在哪里。

而第一部分的代码，也做了如下工作：

将 DS 设置为了 0x10，表示指向了索引值为 2 的全局描述符，即数据段描述符。
将 CS 通过一次长跳转指令设置为了 8，表示指向了索引值为 1 的全局描述符，即代码段描述符。
将 SS:ESP 这个栈顶地址设置为 user_stack 数组的末端。

你看，分段和分页，以及这几个寄存器的设置，其实本质上就是安排我们今后访问内存的方式，做了一个初步规划，包括去哪找代码、去哪找数据、去哪找栈，以及如何通过分段和分页机制将逻辑地址转换为最终的物理地址。

head程序与它们的加载方式有所不同。大致的过程是，先将head.s汇编成目标代码，将用C语言编写的内核程序编译成目标代码，然后链接成system模块。也就是说，system模块里面既有内核程序，又有head程序。两者是紧挨着的。要点是，head程序在前，内核程序在后，所以head程序名字为“head”。head程序在内存中占有25 KB + 184 B的空间。前面讲解过，system模块加载到内存后，setup将system模块复制到0x00000位置，由于head程序在system的前面，所以实际上，head程序就在0x00000这个位置。head程序、以main函数开始的内核程序在system模块中的布局示意图如图。

![main_02](../images/04_main/main_02.jpg#pig_center)

# 规划物理内存格局，设置缓冲区、虚拟盘、主内存

除内核代码和数据所占的内存空间之外，其余物理内存主要分为三部分，分别是**主内存区**、**缓冲区**和**虚拟盘**。**主内存区**是进程代码运行的空间，也包括内核管理进程的数据结构；**缓冲区**主要作为主机与外设进行数据交互的中转站；**虚拟盘区**是一个可选的区域，如果选择使用虚拟盘，就可以将外设上的数据先复制进虚拟盘区，然后加以使用。由于从内存中操作数据的速度远高于外设，因此这样可以提高系统执行效率。

```
#define EXT_MEM_K (*(unsigned short *)0x90002)  /* 1M 以后的扩展内存大小 */

void main(void) 
{
    ROOT_DEV = ORIG_ROOT_DEV;                   /* 0x901FC */
    drive_info = DRIVE_INFO;                    /* 0x90080 */
    
    memory_end = (1<<20) + (EXT_MEM_K<<10);     /* 0xA0020 */
    memory_end &= 0xfffff000;                   /* 0xA0000 */
    if (memory_end > 16*1024*1024)              /* 0x10 0000 */
        memory_end = 16*1024*1024;              /* 内存大于16Mb，则按16Mb */
    if (memory_end > 12*1024*1024)              /* 内存大于12Mb，则设置4Mb缓存 */
        buffer_memory_end = 4*1024*1024;
    else if (memory_end > 6*1024*1024)          /* 内存在6-12 之间，则设置 2Mb内存 */
        buffer_memory_end = 2*1024*1024;        
    else
        buffer_memory_end = 1*1024*1024;        /* 设置1Mb缓存 */
    main_memory_start = buffer_memory_end;
```

其中memory_end为系统有效内存末端位置。超过这个位置的内存部分，在操作系统中不可见。main_memory_start为主内存区起始位置。buffer_memory_end为缓冲区末端位置。

![main_03](../images/04_main/main_03.jpg#pig_center)

# 设置虚拟盘空间并初始化

```
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);  /* 设置虚拟盘区 */
#endif
```

检查文件Makefile中“虚拟盘使用标志”是否设置，以此确定本系统是否使用了虚拟盘。我们设定本书所用计算机有16MB的内存，有虚拟盘，且将虚拟盘大小设置为2 MB。操作系统从缓冲区的末端起开辟2MB内存空间设置为虚拟盘，主内存起始位置后移2MB至虚拟盘的末端。下图展示了设置完成后的物理内存的规划格局。

![main_04](../images/04_main/main_04.jpg#pig_center)


```
#define NR_BLK_DEV	7		// 块设备的数量

/* blk_dev_struct is:
 *	do_request-address
 *	next-request
 */
struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	{ NULL, NULL },		        /* no_dev */          // 0 - 无设备。
	{ NULL, NULL },		        /* dev mem */         // 1 - 内存
	{ NULL, NULL },		        /* dev fd */          // 2 - 软驱设备
	{ NULL, NULL },		        /* dev hd */          // 3 - 硬盘设备
	{ NULL, NULL },		        /* dev ttyx */        // 4 - ttyx 设备
	{ NULL, NULL },		        /* dev tty */         // 5 - tty 设备
	{ NULL, NULL }		        /* dev lp */          // 6 - lp 打印机设备
};

#define DEVICE_REQUEST do_rd_request            // 设备请求函数 do_rd_request()

#define MAJOR_NR 1

char	*rd_start;   /* 虚拟盘在内存中的起始位置。在 52 行初始化函数 rd_init()中     ram_disk_start */
int	rd_length = 0;   /* 虚拟盘所占内存大小 */

long rd_init(long mem_start, int length)
{
	int		i;
	char* 	cp;

	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;    // 第二项挂接
	rd_start = (char *) mem_start;
	rd_length = length;
	cp = rd_start;
	for (i=0; i < length; i++)             //将内存中的虚拟盘区位置全部置0
		*cp++ = '\0';   
	return(length);
}
```

在rd_init()函数中，先要将虚拟盘区的请求项处理函数do_rd_request()与图中的请求项函数控制结构blk_dev[7]的第二项挂接。blk_dev[7]的主要功能是将某一类设备与它对应的请求项处理函数挂钩。可以看出我们讨论的操作系统最多可以管理6类设备。请求项将在后续章节中详细解释。这个挂接动作意味着以后内核能够通过调用do_rd_request函数处理与虚拟盘相关的请求项操作。挂接之后，将虚拟盘所在的内存区域全部初始化为0。下图表示了rd_init()函数的执行效果

![main_05](../images/04_main/main_05.jpg#pig_center)

最后将虚拟盘区的长度值返回。这个返回值将用来重新设置主内存区的起始位置

# 内存管理结构mem_map初始化

```
    mem_init(main_memory_start,memory_end);
```

对主内存区起始位置的重新确定，标志着主内存区和缓冲区的位置和大小已经全都确定了，于是系统开始调用mem_init()函数。先对主内存区的管理结构进行设置，该过程如下图所示

![main_06](../images/04_main/main_06.jpg#pig_center)

```
static long HIGH_MEMORY = 0;

#define LOW_MEM 0x100000							/* 内存低端（1M）*/
#define PAGING_MEMORY (15*1024*1024)				/* 分页内存 15M */
#define PAGING_PAGES (PAGING_MEMORY>>12)            /* 0xf00 = 3840  分页后的页数 */
#define USED 100                                    /* 使用次数 */
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)			/* 计算指定地址的页面号 */

static unsigned char mem_map [ PAGING_PAGES ] = {0,};		// 内存映射字节位图(1 字节代表 1 页)

void mem_init(long start_mem, long end_mem)
{
	int i;

	HIGH_MEMORY = end_mem;
	for (i=0 ; i<PAGING_PAGES ; i++)
		mem_map[i] = USED;
	i = MAP_NR(start_mem);
	end_mem -= start_mem;
	end_mem >>= 12;
	while (end_mem-->0)
		mem_map[i++]=0;
}
```

系统通过mem_map[]对1 MB以上的内存分页进行管理，记录一个页面的使用次数。mem_init()函数先将所有的内存页面使用计数均设置成USED（100，即被使用），然后再将主内存中的所有页面使用计数全部清零，系统以后只把使用计数为0的页面视为空闲页面。

![main_07](../images/04_main/main_07.png#pig_center)

1M 以下的内存这个数组干脆没有记录，这里的内存是无需管理的，或者换个说法是无权管理的，也就是没有权利申请和释放，因为这个区域是内核代码所在的地方，不能被“污染”。

1M 到 2M 这个区间是缓冲区，2M 是缓冲区的末端，缓冲区的开始在哪里之后再说，这些地方不是主内存区域，因此直接标记为 USED，产生的效果就是无法再被分配了。

2M 以上的空间是主内存区域，而主内存目前没有任何程序申请，所以初始化时统统都是零，未来等着应用程序去申请和释放这里的内存资源。

**内核区不能动，缓存区无法被分配，主内存初始化**

# 异常处理类中断服务程序挂接

```

    trap_init();
```

不论是用户进程还是系统内核都要经常使用中断或遇到很多异常情况需要处理，如CPU在参与运算过程中，可能会遇到除零错误、溢出错误、边界检查错误、缺页错误……免不了需要“异常处理”。中断技术也是广泛使用的，系统调用就是利用中断技术实现的。这些中断、异常都需要具体的服务程序来执行。trap_init()函数将中断、异常处理的服务程序与IDT进行挂接，逐步重建中断服务体系，支持内核、进程在主机中的运算。挂接的具体过程及异常处理类中断服务程序在IDT中所占用的位置如图

![main_08](../images/04_main/main_08.jpg#pig_center)

```
// 下面是异常（陷阱）中断程序初始化子程序。设置它们的中断调用门（中断向量）。 
// set_trap_gate()与 set_system_gate()的主要区别在于前者设置的特权级为 0，后者是 3。因此 
// 断点陷阱中断 int3、溢出中断 overflow 和边界出错中断 bounds 可以由任何程序产生。 
// 这两个函数均是嵌入式汇编宏程序(include/asm/system.h,36,39)。


// 设置门描述符宏函数
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

// 设置陷阱门函数
#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

// 设置系统调用门函数
#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

void trap_init(void)
{
	int i;

	set_trap_gate(0,&divide_error);                 /* 除零错误 */
	set_trap_gate(1,&debug);                        /* 单步调试 */
	set_trap_gate(2,&nmi);                          /* 不可屏蔽中断 */
	set_system_gate(3,&int3);	                    /* 中断点陷阱中断 */
	set_system_gate(4,&overflow);                   /* 溢出 */
	set_system_gate(5,&bounds);                     /* 边界检查错误 */
	set_trap_gate(6,&invalid_op);                   /* 无效指令 */
	set_trap_gate(7,&device_not_available);         /* 无效设备 */
	set_trap_gate(8,&double_fault);                 /* 双故障 */
	set_trap_gate(9,&coprocessor_segment_overrun);  /* 协处理器越界 */
	set_trap_gate(10,&invalid_TSS);                 /* 无效TSS */
	set_trap_gate(11,&segment_not_present);         /* 段不存在 */
	set_trap_gate(12,&stack_segment);               /* 栈异常 */
	set_trap_gate(13,&general_protection);          /* 一般性保护异常 */
	set_trap_gate(14,&page_fault);                  /* 缺页 */      
	set_trap_gate(15,&reserved);                    /* 保留 */
	set_trap_gate(16,&coprocessor_error);           /* 协处理器错误  */
	for (i=17;i<48;i++)
		set_trap_gate(i,&reserved);                 /* 挂接，保留中断服务初始化函数 */
	set_trap_gate(45,&irq13);                       /* 协处理器 */
	outb_p(inb_p(0x21)&0xfb,0x21);                  /* 允许IRQ2中断请求 */
	outb(inb_p(0xA1)&0xdf,0xA1);                    /* 允许IRQ3中断请求 */
	set_trap_gate(39,&parallel_interrupt);          /* 设置并口的IDT项 */
}

```

这些代码的目的就是要拼出讲述过的中断描述符。为了便于阅读，复制在下面，如图所示。

![main_09](../images/04_main/main_09.jpg#pig_center)

原理详解:

    #define _set_gate(gate_addr,type,dpl,addr) \
        __asm__ ("movw %%dx,%%ax\n\t" \
            "movw %0,%%dx\n\t" \
            "movl %%eax,%1\n\t" \
            "movl %%edx,%2" \
            : \
            : "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
            "o" (*((char *) (gate_addr))), \
            "o" (*(4+(char *) (gate_addr))), \
            "d" ((char *) (addr)),"a" (0x00080000))


    "d" ((char *) (addr)): 指派本地址给DX寄存器
    "a" (0x00080000):    将EAX寄存器设置为0x00080000

    movw %%dx, %%ax\n\t: 将EDX寄存器中的低位的值放入EAX的低位中
    movw %0,   %%dx\n\t: 将(short)(0x8000+ (dpl<<13) + (type <<8)) 移入EDX寄存器低位中。
    movl %%eax,  %1\n\t: 将EAX寄存器移入(*((char*)(gate_addr))) 中
    movl %%edx,  %2:     将EAX寄存器中的值移入(*(4+(char*)(gate_addr))) 中


“movw %%dx,%%ax\n\t”是把edx的低字赋值给eax的低字；edx是（char *）（addr），也就是&divide_error；eax的值是0x00080000，这个数据在head.s中就提到过，8应该看成1000，每一位都有意义，这样eax的值就是0x00080000+（（char *）（addr）的低字），其中的0x0008是段选择符，含义与第1章中讲解过的“jmpi 0，8”中的8一致。

"movw %0,%%dx\n\t”是把（short）（0x8000 +（dpl<<13）+（type<<8））赋值给dx。别忘了，edx是（char *）（addr），也就是&divide_error。

因为这部分数据是按位拼接的，必须计算精确，我们耐心详细计算一下：

0x8000就是二进制的1000000000000000；

dpl是00，dpl<<13就是000000000000000；

type是15，type<<8就是111100000000；

加起来就是1000111100000000(0x8F00)，这就是DX的值。edx的计算结果就是（char *）（addr）的高字即&divide_error的高字 + 1000111100000000。"movl %%eax,%1\n\t”是把eax的值赋给*（（char *）（gate_addr）），就是赋给idt[0]的前4字节。同理，"movl %%edx,%2”是把edx的值赋给*（4 +（char *）（gate_addr）），就是赋给idt[0]的前后4字节。8字节合起来就是完整的idt[0]。拼接的效果如图所示。

![main_11](../images/04_main/main_11.jpg#pig_center)

![main_12](../images/04_main/main_12.jpg#pig_center)

可以看出，n是0；gate_addr是&idt[0]，也就是idt的第一项中断描述符的地址；type是15；dpl（描述符特权级）是0；addr是中断服务程序divide_error（void）的入口地址。


上述代码的执行效果如图
![main_10](../images/04_main/main_10.jpg#pig_center)


IDT中的第一项除零错误中断描述符初始化完毕，其余异常处理服务程序的中断描述符初始化过程大同小异。后续介绍的所有中断服务程序与IDT的初始化基本上都是以这种方式进行的。set_system_gate（n,addr）与set_trap_gate（n,addr）用的_set_gate（gate_addr,type,dpl,addr）是一样的；差别是set_trap_gate的dpl是0，而set_system_gate的dpl是3。dpl为0的意思是只能由内核处理，dpl为3的意思是系统调用可以由3特权级（也就是用户特权级）调用。

接下来将IDT的int 0x11～int 0x2F都初始化，将IDT中对应的指向中断服务程序的指针设置为reserved（保留）。设置协处理器的IDT项。允许主8259A中断控制器的IRQ2、IRQ3的中断请求。设置并口（可以接打印机）的IDT项。

32位中断服务体系是为适应“被动响应”中断信号机制而建立的。其特点、技术路线是这样的：一方面，硬件产生信号传达给8259A，8259A对信号进行初步处理并视CPU执行情况传递中断信号给CPU；另一方面，CPU如果没有接收到信号，就不断地处理正在执行的程序，如果接收到信号，就打断正在执行的程序并通过IDT找到具体的中断服务程序，让其执行，执行完后，返回刚才打断的程序点继续执行。如果又接收到中断信号，就再次处理中断……

最原始的设计不是这样，那时候CPU每隔一段时间就要对所有硬件进行轮询，以检测它的工作是否完成，如果没有完成就继续轮询，这样就消耗了CPU处理用户程序的时间，降低了系统的综合效率。可见，CPU以“主动轮询”的方式来处理信号是非常不划算的。以“被动响应”模式替代“主动轮询”模式来处理主机与外设的I/O问题，是计算机历史上的一大进步。




```
    blk_dev_init();
    chr_dev_init();
    tty_init();
    time_init();
    sched_init();
    buffer_init(buffer_memory_end);
    hd_init();
    floppy_init();

    sti();
    move_to_user_mode();
    if (!fork()) {
        init();
    }

    for(;;) pause();
}
```
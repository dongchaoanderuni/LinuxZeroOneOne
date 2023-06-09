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

# 初始化块设备请求项结构

```
	blk_dev_init();
```
Linux 0.11将外设分为两类：一类是块设备，另一类是字符设备。块设备将存储空间等分为若干同样大小的称为块的小存储空间，每个块有块号，可以独立、随机读写。硬盘、软盘都是块设备。字符设备以字符为单位进行I/O通信。键盘、早期黑屏命令行显示器都是字符设备。

```
#define NR_REQUEST	32		// 请求（读硬盘）数量

struct request {
	int dev;						/* -1 if no request, -1 就表示空闲*/
	int cmd;						/* READ or WRITE */
	int errors;     				/* 表示操作时产生的错误次数 */
	unsigned long sector;      		/* 表示起始扇区 */
	unsigned long nr_sectors;   	/* 扇区数 */
	char * buffer;              	/* 表示数据缓冲区，也就是读盘之后的数据放在内存中的什么位置*/
	struct task_struct * waiting;   /* task_struct 结构，这可以表示一个进程，也就表示是哪个进程发起了这个请求 */
	struct buffer_head * bh;        /* buffer header, 缓冲区头指针 */
	struct request * next;			/* 指向下一项请求 */
};

/*
 * The request-struct contains all necessary data
 * to load a nr of sectors into memory
 */
struct request request[NR_REQUEST];

void blk_dev_init(void)
{
	int i;

	for (i=0 ; i<NR_REQUEST ; i++) {
		request[i].dev = -1;		/* 依次设置块设备为空闲 */
		request[i].next = NULL;		/* 互不挂接 */
	}
}

```

![main_30](../images/04_main/main_30.png#pig_center)

进程要想与块设备进行沟通，必须经过主机内存中的缓冲区。请求项管理结构request[32]就是操作系统管理缓冲区中的缓冲块与块设备上逻辑块之间读写关系的数据结构。

请求项在进程与块设备进行I/O通信的总体关系如下图所示:

![main_13](../images/04_main/main_13.jpg#pig_center)

操作系统根据所有进程读写任务的轻重缓急，决定缓冲块与块设备之间的读写操作，并把需要操作的缓冲块记录在请求项上，得到读写块设备操作指令后，只根据请求项中的记录来决定当前需要处理哪个设备的哪个逻辑块。


注意：request[32]是一个由数组构成的链表；request[i].dev = −1说明了这个请求项还没有具体对应哪个设备，这个标志将来会被用来判断对应该请求项的当前设备是否空闲；request[i]. next = NULL说明这时还没有形成请求项队列。初始化的过程和效果如图2-12所示。

![main_14](../images/04_main/main_14.jpg#pig_center)

# 初始化字符设备

```
    chr_dev_init();
```
本函数定义为空

```
	void chr_dev_init(void)
	{
	}

```

# 与建立人机交互界面相关的外设的中断服务程序挂接

```
    tty_init();
```

Linus又设计了tty_init()函数，内容就是初始化字符设备。有人解释tty是teletype。字符设备的初始化为进程与串行口（可以通信、连接鼠标……）、显示器以及键盘进行I/O通信准备工作环境，主要是对串行口、显示器、键盘进行初始化设置，以及与此相关的中断服务程序与IDT挂接。

在tty_init()函数中，先调用rs_init()函数来设置串行口，再调用con_init()函数来设置显示器，具体执行代码如下：

```
	// 初始化终端
	void tty_init(void)
	{
		rs_init();          /* 初始化串行中断程序和串行接口 1 和 2。(serial.c, 37) */
		con_init();         /* 初始化控制台终端。(console.c, 617) */
	}

```

```
/* 设置中断门函数 */
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

	/* 串口中断的开启
	/* 初始化串行中断程序和串行接口
	*/
	void rs_init(void)
	{
		set_intr_gate(0x24,rs1_interrupt);	/* 设置串行口1中断 */
		set_intr_gate(0x23,rs2_interrupt);	/* 设置串行口2中断 */
		init(tty_table[1].read_q.data);		/* 初始化串行口1 */
		init(tty_table[2].read_q.data);		/* 初始化串行口2 */
		outb(inb_p(0x21)&0xE7,0x21);		/* 允许IRQ3, IRQ4 */
	}

```

两个串行口中断处理程序与IDT的挂接函数set_intr_gate()与[异常处理类中断服务程序挂接](#异常处理类中断服务程序挂接)介绍过的set_trap_gate()函数类似，可参看前面对set_trap_gate()函数的讲解。它们的差别是set_trap_gate()函数的type是15（二进制的1111），而set_intr_gate()的type是14（二进制的1110）。

```
/* output a value to the port
/* 发送字节到指定端口。 
/* 参数：value - 要写入端口的数据；port - 端口地址
#define outb(value,port) \
__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))

#define outb_p(value,port) \
__asm__ ("outb %%al,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" (value),"d" (port))

/* input a value in port
/* 从指定端口读入字节
/* port - 端口地址
#define inb(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port)); \
_v; \
})


struct tty_struct {
	struct termios termios;   // 终端 io 属性和控制字符数据结构
	int pgrp;                 // 所属进程组。
	int stopped;              // 停止标志
	void (*write)(struct tty_struct * tty);   // tty 写函数指针
	struct tty_queue read_q;                  // tty 读队列
	struct tty_queue write_q;
	struct tty_queue secondary;               // tty 辅助队列(存放规范模式字符序列)
	};

extern struct tty_struct tty_table[];		// tty 结构数组


// port: 串口 1 - 0x3F8，串口 2 - 0x2F8
static void init(int port)
{
	outb_p(0x80,port+3);	/* set DLAB of line control reg */    	/* 设置线路控制寄存器的 DLAB 位(位 7) */
	outb_p(0x30,port);		/* LS of divisor (48 -> 2400 bps) */    /* 发送波特率因子低字节，0x30->2400bps */
	outb_p(0x00,port+1);	/* MS of divisor */
	outb_p(0x03,port+3);	/* reset DLAB */
	outb_p(0x0b,port+4);	/* set DTR,RTS, OUT_2 */
	outb_p(0x0d,port+1);	/* enable all intrs but writes */
	(void)inb(port);		/* read data port to reset things (?) */
}
```
原理详解:

	outb_p():系统级 C 函数，它会向指定的端口发送字节。该函数是在Linux内核中定义的。
			该函数共有两个参数：
			- 第一个参数是要写入端口的数据
			- 第二个参数是端口地址
			该函数内部的 _outb_p() 函数可以保证输出到端口的值不会被编译器优化掉，因此可以正确地将指定数据写入 I/O 端口。
			但需要注意的是，使用 outb_p() 函数需要系统特权 (由于是访问底层硬件)，一般需要在内核态下才能使用，而非用户态。
	outb_p(0x80,port+3):由于0x80的二进制表示（1000 0000）的第7位处于打开状态，因此此按位操作设置第7位到1来启用DLAB位。DLAB(Divisor Latch 	Access Bit 除数闩锁访问点)位是一种标志，指示函数访问波特率设备寄存器。端口+3是线路控制寄存器, 将此值写入该控件，可以让CPU知道我们想要访问DLAB. 这将使我们可以使用另外两个输出命令，以便正确设置波特率。
	outb_p(0x30,port): 使用 outb_p(0x30,port) 向 port 端口发送数据 0x30，从而设置波特率因子低字节。
	outb_p(0x00,port+1): 这两个字节形成一个16位除数值，用于确定串行端口的波特率，这里使得波特率被设定为 2400bps。
	(void)inb(port):表示从给定端口读取1个字节的数据，但是没有对返回结果进行处理
	

![main_15](../images/04_main/main_15s.jpg#pig_center)

```
#define ORIG_VIDEO_PAGE		(*(unsigned short *)0x90004)			/* 显示页面 */
#define ORIG_VIDEO_MODE		((*(unsigned short *)0x90006) & 0xff)	/* 获取显示模式 */
#define ORIG_VIDEO_COLS 	(((*(unsigned short *)0x90006) & 0xff00) >> 8)  /* 在大端模式下获取字符列数*/ 
#define ORIG_VIDEO_EGA_BX	(*(unsigned short *)0x9000a)			/* 显示内存大小和色彩模式 */

#define VIDEO_TYPE_MDA		0x10	/* 单色文本 */
#define VIDEO_TYPE_CGA		0x11	/* CGA 显示器 */
#define VIDEO_TYPE_EGAM		0x20	/* EGA/VGA 的单色模式 */
#define VIDEO_TYPE_EGAC		0x21	/* EGA/VGA 的色彩模式 */


void con_init(void)
{
	register unsigned char a;
	char *display_desc = "????";
	char *display_ptr;

	/* 第一部分 获取显示模式相关信息  */
	video_num_columns = ORIG_VIDEO_COLS;		/* 从加载的设备信息中获取字符列数*/
	video_size_row = video_num_columns * 2;		/* 设置字节对应的列数(字符*2) */
	video_num_lines = ORIG_VIDEO_LINES;			/* 设置显示行数 */
	video_page = ORIG_VIDEO_PAGE;      	 		/* 设置当前显示页面 */
	video_erase_char = 0x0720;					
	
	if (ORIG_VIDEO_MODE == 7)					/* 显卡为单色模式 */
	{
		/* 第二部分 显存映射的内存区域 */
		video_mem_start = 0xb0000;
		video_port_reg = 0x3b4;
		video_port_val = 0x3b5;
		if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
		{
			video_type = VIDEO_TYPE_EGAM;
			video_mem_end = 0xb8000;
			display_desc = "EGAm";
		}
		else
		{
			video_type = VIDEO_TYPE_MDA;
			video_mem_end	= 0xb2000;
			display_desc = "*MDA";
		}
	}
	else										/* If not, it is color. */
	{
		video_mem_start = 0xb8000;
		video_port_reg	= 0x3d4;
		video_port_val	= 0x3d5;
		if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
		{
			video_type = VIDEO_TYPE_EGAC;
			video_mem_end = 0xbc000;
			display_desc = "EGAc";
		}
		else
		{
			video_type = VIDEO_TYPE_CGA;
			video_mem_end = 0xba000;
			display_desc = "*CGA";
		}
	}

	/* Let the user known what kind of display driver we are using */
	/*在屏幕的右上角显示显示描述字符串。采用的方法是直接将字符串写到显示内存的相应位置处。 
 	/* 首先将显示指针 display_ptr 指到屏幕第一行右端差 4 个字符处(每个字符需 2 个字节，因此减 8)
	*/
	display_ptr = ((char *)video_mem_start) + video_size_row - 8;
	while (*display_desc)
	{
		*display_ptr++ = *display_desc++;
		display_ptr++;
	}
	
	/* Initialize the variables used for scrolling (mostly EGA/VGA)	*/
    /* 第三部分 滚动屏幕操作时的信息 */
	origin	= video_mem_start;
	scr_end	= video_mem_start + video_num_lines * video_size_row;
	top	= 0;
	bottom	= video_num_lines;
}
```

**video_erase_char = 0x0720** :该十六进制数可以转换成二进制数0b011100100000，其中前4位0b0111表示背景颜色为亮灰色（Light Gray），后12位0b001000000000表示字符所在单元格使用ASCII码值为' '（空格）。这个二进制数最终被存储在名为video_erase_char的变量中，其用途是作为终端屏幕清除时所使用的默认值。

根据机器系统数据提供的显卡是“单色”还是“彩色”来设置配套信息。由于在Linux 0.11那个时代，大部分显卡器是单色的，所以我们假设显卡的属性是单色EGA。那么显存的位置就要被设置为0xb0000～0xb8000，索引寄存器端口被设置为0x3b4，数据寄存器端口被设置为0x3b5，再将显卡的属性——EGA这三个字符，显示在屏幕上。另外，再初始化一些用于滚屏的变量，其中包括滚屏的起始显存地址、滚屏结束显存地址、最顶端行号以及最低端行号。


```
/* input a value in port with delay
/* 带延迟的从指定端口读入字节
/* port - 端口地址
#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})


static unsigned long	x,y;            /* 当前光标位置。x表示列， y表示行，屏幕左上角为（0,0），右下方为第一象限 */

#define ORIG_X			(*(unsigned char *)0x90000)					/* 光标列号 */
#define ORIG_Y			(*(unsigned char *)0x90001)     			/* 光标行号 */
	/* 第四部分 定位光标并开启键盘中断 */
	gotoxy(ORIG_X,ORIG_Y);							/* 更新光标坐标 */
	set_trap_gate(0x21,&keyboard_interrupt);		/* 设置键盘中断 */
	outb_p(inb_p(0x21)&0xfd,0x21);					/* 取消对键盘中断的屏蔽，允许IRQ1 */
	a=inb_p(0x61);								
	outb_p(a|0x80,0x61);							/* 禁止键盘工作 */
	outb(a,0x61);									/* 允许键盘工作 */
```

**inb_p** :是用来从特定的I/O端口(port)读取一个字节(byte)的函数。
这三行代码启用了键盘中断。inb_p函数从指定的I/O端口读取一个字节，而outb_p将一个字节写入指定的I/O端口。变量a包含键盘控制端口的当前状态，通过位操作将其设置并清除。

**outb()**和**outb_p()**都是用来向指定的端口(寄存器)中写入一个数据byte。两者不同之处在于outb()对I/O的控制不够严格，同时不会等待数据输出成功；而outb_p()随时保证数据能够成功输出，并更加稳定可靠。因此在编写底层设备驱动程序时更应该优先考虑使用outb_p()函数。

```
	/* NOTE! gotoxy thinks x==video_num_columns is ok */
	// 前往新坐标，并更新光标对应现存位置
	static inline void gotoxy(unsigned int new_x,unsigned int new_y)
	{
		/* 如果输入的光标行号超出显示器列数，或者光标行号超出显示的最大行数，则退出。*/
		if (new_x > video_num_columns || new_y >= video_num_lines)
			return;
		x=new_x;
		y=new_y;
		pos=origin + y*video_size_row + (x<<1);   //why x<<1???
}
```

![main_16](../images/04_main/main_16.jpg#pig_center)


![main_17](../images/04_main/main_17.gif#pig_center)

# 开机启动时间设置

```
    time_init();
```

开机启动时间是大部分与时间相关的计算的基础。操作系统中一些程序的运算需要时间参数；很多事务的处理也都要用到时间，比如文件修改的时间、文件最近访问的时间、i节点自身的修改时间等。有了开机启动时间，其他时间就可据此推算出来。

具体执行步骤是：CMOS是主板上的一个小存储芯片，系统通过调用time_init()函数，先对它上面记录的时间数据进行采集，提取不同等级的时间要素，比如秒（time.tm_sec）、分（time.tm_min）、年（time.tm_year）等，然后对这些要素进行整合，并最终得出开机启动时间（startup_time）。

```
struct tm {
	int tm_sec;			/* 秒数[0,59] */
	int tm_min;			/* 分钟数[0,59] */
	int tm_hour;		/* 小时数[0,23] */
	int tm_mday;		/* 月份中的日期[1,31]*/
	int tm_mon;			/* 月份数[0,11] */
	int tm_year;		/* 基于1900年的年数 */
	int tm_wday;		/* 星期中的某天[0,6]，其中星期日为0*/
	int tm_yday;		/* 年份中的某一天[0,365] */
	int tm_isdst;		/* 夏令时标志 */
};

/* 调用该函数前需要向70号寄存器写入0x80|addr，
/* 告知CMOS芯片我们需要获取的数据类型，
/* 并读取71号I/O端口传输的实际数据
*/
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)     /* 从二进制编码的十进制(BCD)转化为16进制 */

extern long startup_time;							// 开机时间。从 1970:0:0:0 开始计时的秒数

	// 初始化时间， 从CMOS芯片中获取
	static void time_init(void)
	{
		struct tm time;

		do {
			time.tm_sec = CMOS_READ(0);             /* 电源关闭时CMOS芯片中的RTC（Real Time Clock）由计算机内部的电池供电，保持活动状态 */
			time.tm_min = CMOS_READ(2);
			time.tm_hour = CMOS_READ(4);
			time.tm_mday = CMOS_READ(7);
			time.tm_mon = CMOS_READ(8);
			time.tm_year = CMOS_READ(9);
		} while (time.tm_sec != CMOS_READ(0));		/* 当等到秒数发生变化时，即时间更新时再读取一次秒钟寄存器的值 */
		BCD_TO_BIN(time.tm_sec);
		BCD_TO_BIN(time.tm_min);
		BCD_TO_BIN(time.tm_hour);
		BCD_TO_BIN(time.tm_mday);
		BCD_TO_BIN(time.tm_mon);
		BCD_TO_BIN(time.tm_year);
		time.tm_mon--;
		startup_time = kernel_mktime(&time);
	}
```

```
#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

/* interestingly, we assume leap-years */
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),                         /* 注意：此行表示默认当年是闰年，闰年2月份为29天，平年为28天 */
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};



	/* 根据刚刚的那些时分秒数据，计算从 1970 年 1 月 1 日 0 时起到开机当时经过的秒数，作为开机时间，存储在 startup_time 这个变量里 */
	long kernel_mktime(struct tm * tm)
	{
		long res;
		int year;

		year = tm->tm_year - 70;
	/* magic offsets (y+1) needed to get leapyears right.*/
		res = YEAR*year + DAY*((year+1)/4);
		res += month[tm->tm_mon];
	/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
		if (tm->tm_mon>1 && ((year+2)%4))
			res -= DAY;
		res += DAY*(tm->tm_mday-1);
		res += HOUR*tm->tm_hour;
		res += MINUTE*tm->tm_min;
		res += tm->tm_sec;
		return res;
	}

```
![main_17](../images/04_main/main_17.jpg#pig_center)

# 初始化进程0

## 初始化进程0

```
    sched_init();

```

进程0是Linux操作系统中运行的第一个进程，也是Linux操作系统父子进程创建机制的第一个父进程。下面讲解的内容对进程0能够在主机中正常运算的影响最为重要和深远，主要包含如下三方面的内容：

1. 系统先初始化进程0。进程0管理结构task_struct的母本（init_task ={INIT_TASK,}）已经在代码设计阶段事先设计好了，但这并不代表进程0已经可用了，还要将进程0的task_struct中的LDT、TSS与GDT相挂接，并对GDT、task[64]以及与进程调度相关的寄存器进行初始化设置。

![main_31](../images/04_main/main_31.png#pig_center)

![main_32](../images/04_main/main_32.png#pig_center)

![main_33](../images/04_main/main_33.png#pig_center)

2. Linux 0.11作为一个现代操作系统，其最重要的标志就是能够支持多进程轮流执行，这要求进程具备参与多进程轮询的能力。系统这里对时钟中断进行设置，以便在进程0运行后，为进程0以及后续由它直接、间接创建出来的进程能够参与轮转奠定基础。
3. 进程0要具备处理系统调用的能力。每个进程在运算时都可能需要与内核进行交互，而交互的端口就是系统调用程序。系统通过函数set_system_gate将system_call与IDT相挂接，这样进程0就具备了处理系统调用的能力了。这个system_call就是系统调用的总入口。进程0只有具备了以上三种能力才能保证将来在主机中正常地运行，并将这些能力遗传给后续建立的进程。这三点的实现都是在sched_init()函数中实现的，具体代码如下：


![main_34](../images/04_main/main_34.png#pig_center)

```
/* 定义了段描述符的数据结构。该结构仅说明每个描述符是由 8 个字节构成，每个描述符表共有 256 项
*/
typedef struct desc_struct {
	unsigned long a,b;
} desc_table[256];

#define SIG_DFL		((void (*)(int))0)	/* default signal handling */  /* 默认的信号处理程序（信号句柄） */
#define SIG_IGN		((void (*)(int))1)	/* ignore signal */            /* 忽略信号的处理程序 */

typedef unsigned int sigset_t;		/* 32 bits  定义信号集类型 */

/* sigaction的数据结构
/* sa_handler 是对应某信号指定要采取的行动。可以是上面的 SIG_DFL，或者是 SIG_IGN 来忽略 
/* 该信号，也可以是指向处理该信号的一个指针。 
/* sa_flags 指定改变信号处理过程的信号集。它是由 37-39 行的位标志定义的
/* 另外，引起触发信号处理的信号也将被阻塞，除非使用了 SA_NOMASK 标志
*/
struct sigaction {
	void (*sa_handler)(int);
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_restorer)(void);
};
```

```
/*在全局表中设置任务状态段/局部表描述符   tss  ldt
*/
#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

```

寄存器AH,AL,AX，EAX的关系如图所示：

```
     +--------+--------+
 ax: |      ah|      al|
     +--------+--------+

     +-----------------+
 eax: |          ax    |
     +-----------------+
```

原理详解：

	"movw $104,%1\n\t":  将 104 存储到内存中地址为 n+0 处（描述符表项的第 1~2 个字节，即 limit 值;
	"movw %%ax,%2\n\t":  将 AX 中的值移动到内存地址为 n+2 处（那么 n+2 和 n+3 所在位置存储的就是由 addr 指定的 TSS/LDT 段选择子）
	"rorl $16,%%eax\n\t" "movb %%al,%3\n\t": 在旋转之后，低位变高位，高位变低位从|eaxAH|eaxAL|axAH|axAL|变成了|axAH|axAL|eaxAH|eaxAL|,将此时al位置(即eaxAL)放入n+4位置
	"movb $" type ",%4\n\t"，"movb $0x00,%5\n\t"： 将type种类移动到内存 n+5 号地址处，最后将 AH 清零；
	"rorl $16,%%eax"：再次将 AX 暂存器右移 16 位，此时高字节为 0，低字节表示段基地址的高字节部分。*(n+6) 和 *(n+7) 存储 TSS/LDT 描述符表项中的高位段基地址；

![main_19](../images/04_main/main_19.jpg#pig_center)

![main_20](../images/04_main/main_20.jpg#pig_center)


```

/* 在全局表中设置任务状态段描述符
*/
#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),addr,"0x89")

#define FIRST_TSS_ENTRY 4						// 全局表中第 1 个任务状态段(TSS)描述符的选择符索引号
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)		// 全局表中第 1 个局部描述符表(LDT)描述符的选择符索引号

/* 任务联合，包括任务结构体和内核堆栈
*/
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))		// 宏定义，计算在全局表中第 n 个任务的 LDT 描述符的索引号

extern unsigned long pg_dir[1024];		// 内存页目录数组。每个目录项为 4 字节。从物理地址 0 开始

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt  */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}

static union task_union init_task = {INIT_TASK,}; // 定义初始任务的数据

	/* 进程调度初始化
	*/
	void sched_init(void)
	{
		int i;
		struct desc_struct * p;

		if (sizeof(struct sigaction) != 16)
			panic("Struct sigaction MUST be 16 bytes");
		set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));	/* 设置初始任务, 非常重要：初始化进程0 */
		set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));	/* 初始化局部描述符表 */
}
```

```
/* 下面是数学协处理器使用的结构，主要用于保存进程切换时 i387 的执行状态信息
*/
struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};
```


```
/* 任务状态段(Task State Segment)，104字节，保存任务信息，任务（进程/线程）切换用，由TR（任务寄存器）寻址。字段构成
/* 1. 寄存器保存区域
/* 2. 内核堆栈指针区域   一个任务可能具有四个堆栈，对应四个特权级。四个堆栈需要四个堆栈指针，3级属于用户态，没有后缀
/* 3. 地址映射寄存器    用于分页寻址，似乎线程切换不需要
/* 4. 链接字段     前一任务的TSS描述符的选择子
/* 5. 其他字段     I/O许可位图
*/
struct tss_struct {
	long	back_link;			/* 16 high bits zero  前一执行任务的TSS任务的描述符 */   
	long	esp0;				/* 内核0级堆栈栈顶指针 */
	long	ss0;				/* 16 high bits zero 0级堆栈栈段寄存器 */		
	long	esp1;				/* 1级堆栈栈顶指针 */
	long	ss1;				/* 16 high bits zero 1级堆栈栈段寄存器 */
	long	esp2;				/* 2级堆栈栈顶指针 */
	long	ss2;				/* 16 high bits zero 2级堆栈栈段寄存器 */
	long	cr3;				/* 控制寄存器3，存储页目录地址 */
	long	eip;				/* 指令寄存器 */
	long	eflags;				/* 标志寄存器 */
	long	eax,ecx,edx,ebx;	/* 通用寄存器 */
	long	esp;				/* 栈顶指针 */
	long	ebp;				/* 栈底指针 */
	long	esi;				/* 源地址 */
	long	edi;				/* 目的地址 */
	long	es;					/* 16 high bits zero 额外段寄存器 */
	long	cs;					/* 16 high bits zero 代码段寄存器 */
	long	ss;					/* 16 high bits zero 栈段寄存器 */
	long	ds;					/* 16 high bits zero 数据段寄存器 */
	long	fs;					/* 16 high bits zero 文件段寄存器 */
	long	gs;					/* 16 high bits zero 额外段寄存器 */
	long	ldt;				/* 16 high bits zero 局部描述符 */
	long	trace_bitmap;		/* bits: trace 0, bitmap 16-31 */  //  当任务进行切换时导致 CPU 产生一个调试(debug)异常的 T-比特位（调试跟踪位）；I/O 比特位图基地址
	struct 	i387_struct i387;
};
```

比源代码、注释和图，可以看出，**movw \$104,\%1**  是将104赋给了段限长15:0的部分；粒度G为0，说明限长就是104字节，而TSS除去struct i387_struct i387后长度正好是104字节。LDT是3×8 =24字节，所以104字节限长够用。TSS的类型是0x89，即二进制的10001001，可以看出**movb $" type,%4**在给type赋值1001的同时，顺便将P、DPL、S字段都赋值好了。同理，movb $0x00,%5在给段限长19:16部分赋值0000的同时，顺便将G、D/B、保留、AVL字段都赋值好了。

![main_18](../images/04_main/main_18.png#pig_center)


进程0的task_struct是由操作系统设计者事先写好的，就是sched.h中的INIT_TASK（参看上面相关源代码和注释，其结构示意见图2-20），并用INIT_TASK的指针初始化task[64]的0项。

![main_21](../images/04_main/main_21.jpg#pig_center)

```
/* 任务（进程）数据结构
*/
struct task_struct {
/* these are hardcoded - don't touch */
	long state;			/* -1 unrunnable, 0 runnable, >0 stopped */	// 任务的运行状态（-1 不可运行，0 可运行(就绪)，>0 已停止）
	long counter;    	/* counter 值的计算方式为 counter = counter /2 + priority */	// 优先执行counter最大的任务;  任务运行时间计数(递减)（滴答数）(时间片)
	long priority;		/* 运行优先数。任务开始运行时 counter = priority，越大运行越长
	long signal;		/* 信号。是位图，每个比特位代表一种信号，信号值=位偏移值+1
	struct sigaction sigaction[32];		/* 信号执行属性结构，对应信号将要执行的操作和标志信息
	long blocked;		/* bitmap of masked signals */  // 进程信号屏蔽码（对应信号位图）
/* various fields */
	int exit_code;													//	任务执行停止的退出码，其父进程会取
	unsigned long start_code,end_code,end_data,brk,start_stack;		// 	代码段地址,代码长度，数据长度，总长度，栈段地址
	long pid,father,pgrp,session,leader;							// 	进程标识号，父进程号，父进程组号，会话号，会话首领
	unsigned short uid,euid,suid;									// 	用户标识号， 有效用户id，保存的用户id
	unsigned short gid,egid,sgid;									// 	组标识号，有效组号，保存的组号
	long alarm;														// 	报警定时器值
	long utime,stime,cutime,cstime,start_time;						// 	用户态运行时间，系统态运行时间，子进程用户态运行时间，子进程系统态运行时间，进程开始运行时刻
	unsigned short used_math;										//	是否使用了协处理器
/* file system info */
	int tty;													/* -1 if no tty, so it must be signed */	// 进程使用 tty 的子设备号。-1 表示没有使用
	unsigned short umask;										// 文件创建属性屏蔽位
	struct m_inode * pwd;										// 当前工作目录i节点
	struct m_inode * root;										// 跟目录i节点
	struct m_inode * executable;								// 执行文件i节点
	unsigned long close_on_exec;								// 执行时关闭文件句柄位图标志
	struct file * filp[NR_OPEN];								// 进程使用的文件表结构，用于保存文件句柄
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];									// 局部描述符段， 0-空，1-代码段 cs，2-数据和堆栈段 ds&ss
/* tss for this task */
	struct tss_struct tss;										// 本进程的任务状态段信息结构
};
```


```
#define NR_TASKS 64		// 系统中同时最多任务（进程）数

		p = gdt+2+FIRST_TSS_ENTRY;
		/* 清任务数组和描述符表项（注意 i=1 开始，所以初始任务的描述符还在）*/
		for(i=1;i<NR_TASKS;i++) {
			task[i] = NULL;
			p->a=p->b=0;
			p++;
			p->a=p->b=0;
			p++;
		}
```

sched_init()函数接下来用for循环将task[64]除进程0占用的0项外的其余63项清空，同时将GDT的TSS1、LDT1往上的所有表项清零，效果如图所示。


```
	/* Clear NT, so that we won't have troubles with that later on */
		__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl"); 	/* 清除标志寄存器中的位 NT，这样以后就不会有麻烦 */
		ltr(0);													/* 给 tr 寄存器赋值， 指向任务状态段 tss */
		lldt(0);												/* 给 ldtr 寄存器赋值, 指向局部描述符表 ldt */ 
```

原理详解:

	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl"): 将标志寄存器入栈，清除中断使能标志位(NT)即第14位，由此将去使能中断，之后将修改值出栈。
	ltr(0)：将GDT中第一个条目的TSS（任务状态段）结构加载到任务寄存器（tr）中。 tr寄存器保存与当前任务的TSS对应的段选择器（在以后切换到该任务时操作系统会使用它）。
	lldt(0)：将GDT中第一个条目的LDT（本地描述符表）结构加载到本地描述符表寄存器（ldtr）中。当进程运行时，MMU将其逻辑地址发送到LDTR，LDTR将虚拟地址映射到物理地址。


![main_22](../images/04_main/main_22.jpg#pig_center)


初始化进程0相关的管理结构的最后一步是非常重要的一步，是将TR寄存器指向TSS0、LDTR寄存器指向LDT0，这样，CPU就能通过TR、LDTR寄存器找到进程0的TSS0、LDT0，也能找到一切和进程0相关的管理信息。


## 设置时钟中断

接下来就对时钟中断进行设置。时钟中断是进程0及其他由它创建的进程轮转的基础。对时钟中断进行设置的过程具体分为如下三个步骤。

```
#define LATCH (1193180/HZ)				//  定义每个时间片的滴答数

		/* 下面代码用于初始化 8253 定时器 
		*/
		outb_p(0x36,0x43);								/* binary, mode 3, LSB/MSB, ch 0 */ 
		outb_p(LATCH & 0xff , 0x40);					/* LSB */
		outb(LATCH >> 8 , 0x40);						/* MSB */             //  这四行代码就开启了这个定时器，之后这个定时器变会持续的、以一定频率的向 CPU 发出中断信号， 中断处理程序为 timer_interrupt
		set_intr_gate(0x20,&timer_interrupt);
		outb(inb_p(0x21)&~0x01,0x21);
```


![main_36](../images/04_main/main_36.gif#pig_center)

1. 对支持轮询的8253定时器进行设置。这一步操作如图中的第一步所示，其中LATCH最关键。LATCH是通过一个宏定义的，通过它在sched.c中的定义“#define LATCH（1193180/HZ）”，即系统每10毫秒发生一次时钟中断。
2. 设置时钟中断，如图中的第二步所示，timer_interrupt()函数挂接后，在发生时钟中断时，系统就可以通过IDT找到这个服务程序来进行具体的处理。
3. 将8259A芯片中与时钟中断相关的屏蔽码打开，时钟中断就可以产生了。从现在开始，时钟中断每1/100秒就产生一次。由于此时处于“关中断”状态，CPU并不响应，但进程0已经具备参与进程轮转的潜能。

![main_23](../images/04_main/main_23.jpg#pig_center)


## 设置系统调用总入口

```
		set_system_gate(0x80,&system_call);          // 所有用户态程序想要调用内核提供的方法，都需要基于这个系统调用来进行
```

将系统调用处理函数system_call与int 0x80中断描述符表挂接。system_call是整个操作系统中系统调用软中断的总入口。所有用户程序使用系统调用，产生int 0x80软中断后，操作系统都是通过这个总入口找到具体的系统调用函数。该过程如图所示:

![main_24](../images/04_main/main_24.jpg#pig_center)

系统调用函数是操作系统对用户程序的基本支持。在操作系统中，依托硬件提供的特权级对内核进行保护，不允许用户进程直接访问内核代码。但进程有大量的像读盘、创建子进程之类的具体事务处理需要内核代码的支持。为了解决这个矛盾，操作系统的设计者提供了系统调用的解决方案，提供一套系统服务接口。用户进程只要想和内核打交道，就调用这套接口程序，之后，就会立即引发int 0x80软中断，后面的事情就不需要用户程序管了，而是通过另一条执行路线——由CPU对这个中断信号响应，翻转特权级（从用户进程的3特权级翻转到内核的0特权级），通过IDT找到系统调用端口，调用具体的系统调用函数来处理事务，之后，再iret翻转回到进程的3特权级，进程继续执行原来的逻辑，这样矛盾就解决了。

# 初始化缓冲区管理结构


缓冲区是内存与外设（如硬盘，以后以硬盘为例）进行数据交互的媒介。内存与硬盘最大的区别在于，硬盘的作用仅仅是对数据信息以很低的成本做大量数据的断电保存，并不参与运算（因为CPU无法到硬盘上进行寻址），而内存除了需要对数据进行保存以外，更重要的是要与CPU、总线配合进行数据运算。**缓冲区则介于两者之间，它既对数据信息进行保存，也能够参与一些像查找、组织之类的间接、辅助性运算**。有了缓冲区这个媒介以后，对外设而言，它仅需要考虑与缓冲区进行数据交互是否符合要求，而不需要考虑内存如何使用这些交互的数据；对内存而言，它也仅需要考虑与缓冲区交互的条件是否成熟，而不需要关心此时外设对缓冲区的交互情况。两者的组织、管理和协调将由操作系统统一操作。

操作系统通过hash_table[NR_HASH]、buffer_head双向环链表组成的复杂的哈希表管理缓冲区。操作系统通过调用buffer_init()函数对缓冲区进行设置，执行代码如下：

```
	buffer_init(buffer_memory_end);
```

![main_37](../images/04_main/main_37.png#pig_center)

```

/* 缓冲区头数据结构 
/* 在程序中常用 bh 来表示 buffer_head 类型的缩写
*/
struct buffer_head {
	char* b_data;							/* pointer to data block (1024 bytes) 数据块 */
	unsigned long b_blocknr;				/* block number 块号 */
	unsigned short b_dev;					/* device (0 = free) 数据源的设备号 */
	unsigned char b_uptodate;  				/* 更新标志，表示数据是否已经更新 */
	unsigned char b_dirt;					/* 0-clean,1-dirty */
	unsigned char b_count;					/* users using this block */  /* 使用的用户数, reference count */ 
	unsigned char b_lock;					/* 0 - ok, 1 -locked */
	struct task_struct * b_wait;          	/* 指向等待该缓冲区解锁的任务 */
	struct buffer_head * b_prev;			/* 前一块（这四个指针用于缓冲区的管理）*/
	struct buffer_head * b_next;			/* 下一块 */
	struct buffer_head * b_prev_free;		/* 前一空闲块 */
	struct buffer_head * b_next_free;		/* 下一空闲块 */
};

/* 外部传入，动态生成  整个内核代码的末尾地址 */
extern int end;                   
struct buffer_head * start_buffer = (struct buffer_head *) &end;

#define BLOCK_SIZE 1024                   	/* 数据块长度 */

int NR_BUFFERS = 0;

struct buffer_head * hash_table[NR_HASH];

#define NR_HASH 307                       	/* 缓冲区hash表大小 */

void buffer_init(long buffer_end)
{
	struct buffer_head * h = start_buffer;
	void * b;
	int i;
/* 如果缓冲区高端等于 1Mb，则由于从 640KB-1MB 被显示内存和 BIOS 占用，因此实际可用缓冲区
/* 内存 
/* 高端应该是 640KB。否则内存高端一定大于 1MB
*/
	if (buffer_end == 1<<20)      //1M
		b = (void *) (640*1024);   // = A0000,  640k
	else
		b = (void *) buffer_end;    // 这里采用2M为例
	/* 当末端比内核末端+1（即）缓冲区开始位置大于等于至少一页时候 */
	while ( (b -= BLOCK_SIZE) >= ((void *) (h+1)) ) {    //BLOCK_SIZE = 1024
		h->b_dev = 0;					
		h->b_dirt = 0;
		h->b_count = 0;
		h->b_lock = 0;
		h->b_uptodate = 0;
		h->b_wait = NULL;
		h->b_next = NULL;				// 这两项初始化为空，后续的使用将与hast_table挂接
		h->b_prev = NULL;
		h->b_data = (char *) b;         // 每个buffer_head关联一个缓冲块（1024 字节）
		h->b_prev_free = h-1;			// 使得buffer_head分别与前
		h->b_next_free = h+1;			// 后buffer_head挂接，形成双向链表
		h++;
		NR_BUFFERS++;                   // 缓冲区块数累加， 初始为0
		if (b == (void *) 0x100000)     // 如果地址 b 递减到等于 1MB，则跳过 384KB， 让 b 指向地址 0xA0000(640KB)处，因为640k-1M 存有ROMBIOS&VGA信息
			b = (void *) 0xA0000;
	}
	h--;                                // 让 h 指向最后一个有效缓冲头
	free_list = start_buffer;           // 让空闲链表头指向头一个缓冲区头 
	free_list->b_prev_free = h;
	h->b_next_free = free_list;         // 形成一个环链
	for (i=0;i<NR_HASH;i++)             // 初始化 hash 表（哈希表、散列表），置表中所有的指针为 NULL
		hash_table[i]=NULL;
}	

```

buffer_memory_end为缓冲区末端位置。

![main_03](../images/04_main/main_03.jpg#pig_center)

每个buffer_head的大小为32字节，每个block大小为1024字节，则可分配缓冲区对数为(3FFFFF-A0000)/(0x400+0x20) = 3389对

![main_38](../images/04_main/main_38.png#pig_center)

在buffer_init()函数里，从内核的末端及缓冲区的末端同时开始，方向相对增长、配对地做出buffer_head、缓冲块，直到不足一对buffer_head、缓冲块。在第2章开始时设定的内存格局（16M内存，1MB内核内存，3MB缓存，2MB虚拟盘，9MB主内存）下，有3000多对buffer_head、缓冲块，buffer_head在低地址端，缓冲块在高地址端。

![main_39](../images/04_main/main_39.png#pig_center)

将buffer_head的成员设备号b_dev、引用次数b_count、“更新”标志b_uptodate、“脏”标志b_dirt、“锁定”标志b_lock设置为0。如图2-24所示，将b_data指针指向对应的缓冲块。利用buffer_head的b_prev_free、b_next_free，将所有的buffer_head形成双向链表。使free_list指向第一个buffer_head，并利用free_list将buffer_head形成双向链表链接成双向环链表，如图所示。

![main_40](../images/04_main/main_40.png#pig_center)

![main_27](../images/04_main/main_27.jpg#pig_center)

![main_26](../images/04_main/main_26.jpg#pig_center)

注意下图顶部所示的内存的变化。在紧靠系统内核的部分，多出了一块用黑色表示的内存区域，那里面存储的就是缓冲区管理结构。由于它管理着3000多个缓冲块，因此它占用的内存空间的大小，与内核几乎差不多。图中也对空闲表的双向链表结构给出了形象的说明。

![main_25](../images/04_main/main_25.jpg#pig_center)

这个end就是内核代码末端的地址。在代码编写阶段，设计者事先较难准确估算这个地址，于是就在内核模块链接期间设置end这个值，然后在这里使用。

最后，对hash_table[307]进行设置，将hash_table[307]的所有项全部设置为NULL。同时，又用一个 hashmap 结构，索引到所有缓冲头，方便快速查找，为之后的通过 LRU 算法使用缓冲区做准备。

![main_41](../images/04_main/main_41.png#pig_center)


# 初始化硬盘

```
    hd_init();

```

硬盘的初始化为进程与硬盘这种块设备进行I/O通信建立了环境基础。

在hd_init()函数中，将硬盘请求项服务程序do_hd_request()与blk_dev控制结构相挂接，硬盘与请求项的交互工作将由do_hd_request()函数来处理，然后将硬盘中断服务程序hd_interrupt()与IDT相挂接，最后，复位主8259A int2的屏蔽位，允许从片发出中断请求信号，复位硬盘的中断请求屏蔽位（在从片上），允许硬盘控制器发送中断请求信号。

```
#define MAJOR_NR 3		// 硬盘主设备号是 3
#define DEVICE_REQUEST do_hd_request

struct request {
	int dev;						/* -1 if no request, -1 就表示空闲*/
	int cmd;						/* READ or WRITE */
	int errors;     				/* 表示操作时产生的错误次数 */
	unsigned long sector;      		/* 表示起始扇区 */
	unsigned long nr_sectors;   	/* 扇区数 */
	char * buffer;              	//表示数据缓冲区，也就是读盘之后的数据放在内存中的什么位置
	struct task_struct * waiting;   /* task_struct 结构，这可以表示一个进程，也就表示是哪个进程发起了这个请求
	struct buffer_head * bh;        /* buffer header  缓冲区头指针
	struct request * next;			/* 指向下一项请求
};
```



```

struct blk_dev_struct {
	void (*request_fn)(void);			// 请求操作的函数指针，可以使硬盘，软盘或者其他
	struct request * current_request;	// 请求信息结构
};

	/* 硬盘系统初始化 */
	void hd_init(void)
	{
		blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;	// do_hd_request()
		set_intr_gate(0x2E,&hd_interrupt);          	//set interrupt
		outb_p(inb_p(0x21)&0xfb,0x21);      			//往几个 IO 端口上读写，其作用是允许硬盘控制器发送中断请求信号
		outb(inb_p(0xA1)&0xbf,0xA1);
	}

```

![main_28](../images/04_main/main_28.jpg#pig_center)

```

#define CURRENT (blk_dev[MAJOR_NR].current_request)	 	// CURRENT 为指定主设备号的当前请求结构

#define MAJOR(a) (((unsigned)(a))>>8)     // 取高字节（主设备号）
#define MINOR(a) ((a)&0xff)               // 取低字节（次设备号）

// 检查指针，设备号，是否锁定
#define INIT_REQUEST \
repeat: \
	if (!CURRENT) \											/* 当前请求不存在 */
		return; \
	if (MAJOR(CURRENT->dev) != MAJOR_NR) \					/* 当前设备与调用请求设备号不符 */
		panic(DEVICE_NAME ": request list destroyed"); \
	if (CURRENT->bh) { \									/* 当前缓冲区头指针为空 */
		if (!CURRENT->bh->b_lock) \							/* 当前缓冲区未被锁定 */
			panic(DEVICE_NAME ": block not locked"); \
	}
// 如果在进行请求操作时缓冲区没锁定则死机
#endif

/*
 *  This struct defines the HD's and their types.
 *  各字段分别是磁头数、每磁道扇区数、柱面数、写前预补偿柱面号、磁头着陆区柱面号、控制字节。
 */
struct hd_i_struct {
	int head,sect,cyl,wpcom,lzone,ctl;
	};
#ifdef HD_TYPE
struct hd_i_struct hd_info[] = { HD_TYPE };
#define NR_HD ((sizeof (hd_info))/(sizeof (struct hd_i_struct)))
#else
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

/* 定义硬盘分区结构。给出每个分区的物理起始扇区号、分区扇区总数。 
/* 其中 5 的倍数处的项（例如 hd[0]和 hd[5]等）代表整个硬盘中的参数。
*/
static struct hd_struct {
	long start_sect;
	long nr_sects;
} hd[5*MAX_HD]={{0,0},};
```

```
/* 将状态置为 TASK_RUNNING
*/
void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;                //判空后，state切换为运行状态，释放当前等待的任务指针；
	}
}

/* 解锁缓冲区头bh
*/
static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;
	wake_up(&bh->b_wait);		// 唤醒等待该缓冲区的任务
}

/*
 * used to wait on when there are no free requests
 * 等待空闲请求的链表
 */
struct task_struct * wait_for_request = NULL;

/* 结束请求
*/
extern inline void end_request(int uptodate)
{
	DEVICE_OFF(CURRENT->dev);
	if (CURRENT->bh) {							// 当前缓冲区头指针不为空
		CURRENT->bh->b_uptodate = uptodate;		// 置更新标志
		unlock_buffer(CURRENT->bh);				// 释放缓冲区
	}
	if (!uptodate) {									// 如果更新状态为无效值
		printk(DEVICE_NAME " I/O error\n\r");	
		printk("dev %04x, block %d\n\r",CURRENT->dev,
			CURRENT->bh->b_blocknr);
	}
	wake_up(&CURRENT->waiting);					// 唤醒等待任务
	wake_up(&wait_for_request);									
	CURRENT->dev = -1;                 			// 表示当前设备空闲
	CURRENT = CURRENT->next;					// 转移到下一个请求
}

	// 执行硬盘读写请求操作
	void do_hd_request(void)
	{
		int i,r;
		unsigned int block,dev;
		unsigned int sec,head,cyl;
		unsigned int nsect;

		INIT_REQUEST;						// 检查请求的合法性
		dev = MINOR(CURRENT->dev);
		block = CURRENT->sector;			// 起始扇区数

		/* 该代码中的 dev 变量设置为从请求中提取的次设备号，block 设置为该请求所在的扇区号。
		 * 接下来的代码检查 dev 的值是否大于或等于5 * NR_HD或者结束扇区（定义为当前块+2）是* 否超出了该硬盘的总扇区数。如果这两个条件任何一个成立，函数将以错误方式结束该请求，* 并跳转到标记 repeat 处。
		*/
		if (dev >= 5*NR_HD || block+2 > hd[dev].nr_sects) {
			end_request(0);
			goto repeat;                 	//并跳转到标号 repeat 处, （定义在 INIT_REQUEST 开始处）
		}

		/* block更新为物理起始扇区号加上原来的block值。然后将dev变量除以5以获取相应的硬盘**号。
		*/
		block += hd[dev].start_sect;
		dev /= 5;

		/* 下面嵌入汇编代码用来从硬盘信息结构中根据起始扇区号和每磁道扇区数计算在磁道中的 
		/* 扇区号(sec)、所在柱面号(cyl)和磁头号(head)
		*/
		__asm__("divl %4":"=a" (block),"=d" (sec):"0" (block),"1" (0),
			"r" (hd_info[dev].sect));
		__asm__("divl %4":"=a" (cyl),"=d" (head):"0" (block),"1" (0),
			"r" (hd_info[dev].head));
```

这行代码是一个内嵌的汇编，用于在硬盘信息结构hd_info中获取当前设备dev的每磁道扇区数，并使用除法指令divl将当前请求的扇区号分解出要访问的柱面、磁头和扇区。

	被除数：((edx<<32)|eax):block
	除数: hd[dev].start_sect(物理起始扇区号)
	商: block(块区)
	余数: sec(扇区号)

类似的第二个内嵌汇编得到的是

	被除数：((edx<<32)|eax):block
	除数: hd_info[dev].head(磁头号)
	商:   cyl(柱面号)
	余数: head(磁头号)


```
static int reset = 0;	// 全局复位变量

#define HD_ERROR	0x1f1	/* see err-bits */
#define HD_STATUS	0x1f7	/* see status-bits */

#define HD_CMD		0x3f6

#define nop() __asm__ ("nop"::)		// 空操作

#define SEEK_STAT	0x10
#define READY_STAT	0x40
#define BUSY_STAT	0x80

// 等待硬盘就绪。
static int drive_busy(void)
{
	unsigned int i;

	for (i = 0; i < 10000; i++)
		if (READY_STAT == (inb_p(HD_STATUS) & (BUSY_STAT|READY_STAT)))
			break;
	i = inb(HD_STATUS);
	i &= BUSY_STAT | READY_STAT | SEEK_STAT;
	if (i == READY_STAT | SEEK_STAT)
		return(0);
	printk("HD controller times out\n\r");
	return(1);
}


// 诊断复位（重新校正）硬盘控制器
static void reset_controller(void)
{
	int	i;

	outb(4,HD_CMD);               				// 向控制寄存器端口发送控制字节(4-复位)。
	for(i = 0; i < 100; i++) nop();    			// 等待一段时间（循环空操作）
	outb(hd_info[0].ctl & 0x0f ,HD_CMD);       	// 再发送正常的控制字节(不禁止重试、重读)
	if (drive_busy())
		printk("HD-controller still busy\n\r");
	if ((i = inb(HD_ERROR)) != 1)
		printk("HD-controller reset failed: %02x\n\r",i);
}


#define WRERR_STAT	0x20

#define HD_DATA		0x1f0	/* _CTL when writing */

/* 向硬盘控制器发送命令块  写端口
*/
static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
		unsigned int head,unsigned int cyl,unsigned int cmd,
		void (*intr_addr)(void))
{
	register int port asm("dx");

	if (drive>1 || head>15)
		panic("Trying to write bad sector");
	if (!controller_ready())
		panic("HD controller not ready");
	do_hd = intr_addr;
	outb_p(hd_info[drive].ctl,HD_CMD);
	port=HD_DATA;
	outb_p(hd_info[drive].wpcom>>2,++port);
	outb_p(nsect,++port);
	outb_p(sect,++port);
	outb_p(cyl,++port);
	outb_p(cyl>>8,++port);
	outb_p(0xA0|(drive<<4)|head,++port);
	outb(cmd,++port);
}

/* 检测硬盘执行命令后的状态。(win_表示温切斯特硬盘的缩写)
*/
static int win_result(void)
{
	int i=inb_p(HD_STATUS);   //status is in some register

	if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))
		== (READY_STAT | SEEK_STAT))
		return(0); /* ok */
	if (i&1) i=inb(HD_ERROR);    // 若 ERR_STAT 置位，则读取错误寄存器
	return (1);
}

/* 读写硬盘失败处理调用函数
*/
static void bad_rw_intr(void)
{
	if (++CURRENT->errors >= MAX_ERRORS)
		end_request(0);
	if (CURRENT->errors > MAX_ERRORS/2)
		reset = 1;
}


/* 硬盘重新校正（复位）中断调用函数
*/
static void recal_intr(void)
{
	if (win_result())
		bad_rw_intr();
	do_hd_request();
}


static void reset_hd(int nr)
{
	reset_controller();
	hd_out(nr,hd_info[nr].sect,hd_info[nr].sect,hd_info[nr].head-1,
		hd_info[nr].cyl,WIN_SPECIFY,&recal_intr);
}

#define WIN_WRITE		0x30
```

```
/* 写端口 port，共写 nr 字，从 buf 中取数据。
*/
#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw"::"d" (port),"S" (buf),"c" (nr):"cx","si")
```

这段代码是C语言中的宏定义，它定义了一个名为port_write的宏，并用内嵌汇编实现了向指定端口写入数据的功能。

该内嵌汇编使用了“rep; outsw”指令序列来将buf所指向的nr个字节的内容写入到端口号为port的设备中。其中，操作数d、S、c分别被赋值为(port)、(buf)、(nr)；而cx和si寄存器则被作为汇编指令的默认操作数，即程序不直接使用这两个寄存器。

```
#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif

#define DEVICE_INTR do_hd                         // 设备中断处理程序 do_hd()。

// 写扇区中断调用函数。
static void write_intr(void)
{
	if (win_result()) {
		bad_rw_intr();
		do_hd_request();
		return;
	}
	if (--CURRENT->nr_sectors) {
		CURRENT->sector++;
		CURRENT->buffer += 512;
		do_hd = &write_intr;
		port_write(HD_DATA,CURRENT->buffer,256);
		return;
	}
	end_request(1);
	do_hd_request();
}

#define WIN_READ		0x20

// 读操作中断调用函数
static void read_intr(void)
{
	if (win_result()) {
		bad_rw_intr();
		do_hd_request();
		return;
	}
	port_read(HD_DATA,CURRENT->buffer,256);
	CURRENT->errors = 0;
	CURRENT->buffer += 512;
	CURRENT->sector++;
	if (--CURRENT->nr_sectors) {
		do_hd = &read_intr;
		return;
	}
	end_request(1);    // 若全部扇区数据已经读完，则处理请求结束事宜
	do_hd_request();
}

		sec++;
		nsect = CURRENT->nr_sectors;		// 获得当前所在扇区号
		/* 如果 reset 置 1，则执行复位操作。复位硬盘和控制器，并置需要重新校正标志，返回
		*/
		if (reset) {
			reset = 0;
			recalibrate = 1;
			reset_hd(CURRENT_DEV);
			return;
		}
		/* 如果重新校正标志(recalibrate)置位，则首先复位该标志，然后向硬盘控制器发送重新 
		/* 校正命令
		*/
		if (recalibrate) {
			recalibrate = 0;
			hd_out(dev,hd_info[CURRENT_DEV].sect,0,0,0,
				WIN_RESTORE,&recal_intr);
			return;
		}	
		if (CURRENT->cmd == WRITE) {
			hd_out(dev,nsect,sec,head,cyl,WIN_WRITE,&write_intr);
			for(i=0 ; i<3000 && !(r=inb_p(HD_STATUS)&DRQ_STAT) ; i++)
				/* nothing */ ;
			if (!r) {
				bad_rw_intr();
				goto repeat;
			}
			port_write(HD_DATA,CURRENT->buffer,256);
		} else if (CURRENT->cmd == READ) {
			hd_out(dev,nsect,sec,head,cyl,WIN_READ,&read_intr);
		} else		
			panic("unknown hd-command");	// 未知命令，宕机
	}

```

# 开启软盘

```
     floppy_init();
```
这东西都没人用了，算了算了。

# 开启中断

```
   sti();
```

现在，系统中所有中断服务程序都已经和IDT正常挂接。这意味着中断服务体系已经构建完毕，系统可以在32位保护模式下处理中断，重要意义之一是可以使用系统调用。

下图给出了开中断后的效果，注意其中EFLAGS中的变化。


![main_29](../images/04_main/main_29.jpg#pig_center)

目前开启的中断

|中断号	|中断处理函数|
|:-:|:-:|
|0-0x10	|trap_init里设置的一堆|
|0x20	|timer_interrupt|
|0x21	|keyboard_interrupt|
|0x2E	|hd_interrupt|
|0x80	|system_call|


看到最后，你会发现操作系统就是一个靠中断驱动的死循环而已，如果不发生任何中断，操作系统会一直在一个死循环里等待。换句话说，让操作系统工作的唯一方式，就是触发中断。

# 进程0由0特权级翻转到3特权级，成为真正的进程

Linux操作系统规定，除进程0之外，所有进程都要由一个已有进程在3特权级下创建。在Linux 0.11中，进程0的代码和数据都是由操作系统的设计者写在内核代码、数据区，并且，此前处在0特权级，严格说还不是真正意义上的进程。为了遵守规则，在进程0正式创建进程1之前，要将进程0由0特权级转变为3特权级。方法是调用move_to_user_mode()函数，模仿中断返回动作，实现进程0的特权级从0转变为3。

```
    move_to_user_mode();
```

```

/* 切换到用户模式运行。 
 * 该函数利用 iret 指令实现从内核模式切换到用户模式（初始任务 0）
*/
#define move_to_user_mode() \		// 模仿中断硬件压栈，顺序是ss, esp, eflags, cs, eip
__asm__ ("movl %%esp,%%eax\n\t" \	
	"pushl $0x17\n\t" \				// SS入栈，0x17(10111)(3特权级、LDT、数据段)
	"pushl %%eax\n\t" \				// ESP入栈
	"pushfl\n\t" \					// EFLAGS 入栈
	"pushl $0x0f\n\t" \				// CS入栈,  0x0F(1111)(3特权级、LDT、代码段)
	"pushl $1f\n\t" \				// EIP入栈
	"iret\n" \						// 出栈恢复现场、翻转特权级别从0到3
	"1:\tmovl $0x17,%%eax\n\t" \	// 下面代码使ds,es,fs,gs,ss一致
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")
```
中断与函数调用不同的是，函数调用是程序员事先设计好的，知道在代码的哪个地方调用，编译器可以预先编译出压栈保护现场和出栈恢复现场的代码；而中断的发生是不可预见的，无法预先编译出保护、恢复的代码，只好由硬件完成保护、恢复的压栈、出栈动作。所以，int指令会引发CPU硬件完成SS、ESP、EFLAGS、CS、EIP的值按序进栈，同理，CPU执行iret指令会将栈中的值自动按反序恢复给这5个寄存器。

![main_29](../images/04_main/main_29.jpg#pig_center)

CPU响应中断的时候，根据DPL的设置，可以实现指定的特权级之间的翻转。前面的sched_init函数中的set_system_gate（0x80,&system_call）就是设置的int 0x80中断由3特权级翻转到0特权级，3特权级的进程做了系统调用int 0x80，CPU就会翻转到0特权级执行系统代码。同理，iret又会从0特权级的系统代码翻转回3特权级执行进程代码。

![main_19](../images/04_main/main_19.jpg#pig_center)


move_to_user_mode()函数就是根据这个原理，利用iret实现从0特权级翻转到3特权级。

由于进程0的代码到现在一直处在0特权级，并不是从3特权级通过int翻转到0特权级的，栈中并没有int自动压栈的5个寄存器的值。为了iret的正确使用，设计者手工写压栈代码模拟int的压栈，当执行iret指令时，CPU自动将这5个寄存器的值按序恢复给CPU，CPU就会翻转到3特权级的段，执行3特权级的进程代码。

为了iret能翻转到3特权级，不仅手工模拟的压栈顺序必须正确，而且SS、CS的特权级还必须正确。注意：栈中的SS值是0x17，用二进制表示就是00010111，最后两位表示3，是用户特权级，倒数第3位是1，表示从LDT中获取段描述符，第4～5位的10表示从LDT的第3项中得到进程栈段的描述符。

当执行iret时，硬件会按序将5个push压栈的数据分别出栈给SS、ESP、EFLAGS、CS、EIP。压栈顺序与通常中断返回时硬件的出栈动作一样，返回的效果也是一样的。

执行完move_to_user_mode( )，相当于进行了一次中断返回，进程0的特权级从0翻转为3，成为名副其实的进程。

这样 CPU 就开始可以接收并处理中断信号了，键盘可以按了，硬盘可以读写了，时钟可以震荡了，系统调用也可以生效了！

这就代表着，操作系统具有了控制台交互能力，硬盘读写能力，进程调度能力，以及响应用户进程的系统调用请求！

![main_42](../images/04_main/main_42.png#pig_center)

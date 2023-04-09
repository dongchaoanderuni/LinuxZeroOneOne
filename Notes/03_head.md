本文解析head.s程序，主要分为 部分:
- 1 设置中断描述表(IDT)
- 2 设置全局描述符表(GDT)
- 3 检测A20总线打开
- 4 检测协处理器
- 5 开启分页机制
- 6 调用主函数
- 7 地址再探


在执行main函数之前，先要执行三个由汇编代码生成的程序，即bootsect、setup和head。之后，才执行由main函数开始的用C语言编写的操作系统内核程序。前面我们讲过，第一步，加载bootsect到0x07C00，然后复制到0x90000；第二步，加载setup到0x90200。值得注意的是，这两段程序是分别加载、分别执行的。

head程序与它们的加载方式有所不同。大致的过程是，先将head.s汇编成目标代码，将用C语言编写的内核程序编译成目标代码，然后链接成system模块。也就是说，system模块里面既有内核程序，又有head程序。两者是紧挨着的。要点是，head程序在前，内核程序在后，所以head程序名字为“head”。head程序在内存中占有25 KB + 184 B的空间。前面讲解过，system模块加载到内存后，setup将system模块复制到0x00000位置，由于head程序在system的前面，所以实际上，head程序就在0x00000这个位置。head程序、以main函数开始的内核程序在system模块中的布局示意图如图

![head_01](../images/03_head/head_01.jpg#pig_center)

head程序除了做一些调用main的准备工作之外，还做了一件对内核程序在内存中的布局及内核程序的正常运行有重大意义的事，就是用程序自身的代码在程序自身所在的内存空间创建了内核分页机制，即在0x000000的位置创建了页目录表、页表、缓冲区、GDT、IDT，并将head程序已经执行过的代码所占内存空间覆盖。这意味着head程序自己将自己废弃，main函数即将开始执行。

# 设置中断描述表(IDT)

```
.text
.globl _idt,_gdt,_pg_dir,_tmp_floppy_area
_pg_dir:
```

**_pg_dir**: 标识内核分页机制完成后的内核起始位置，也就是物理内存的起始位置0x000000。head程序马上就要在此处建立页目录表，为分页机制做准备。这一点非常重要，是内核能够掌控用户进程的基础之一。

```
startup_32:
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
```

给EAX寄存器赋值，然后DS，ES，FS，GS寄存器的值均变为0x10(二进制下的0001 0000)，最后三位与前面讲解的一样，其中最后两位（00）表示内核特权级，从后数第3位（0）表示选择GDT，第4、5两位（10）是GDT的2项，也就是第3项。也就是说，4个寄存器用的是同一个全局描述符，它们的段基址、段限长、特权级都是相同的。特别要注意的是，影响段限长的关键字段的值是0x7FF，段限长就是8 MB。

```
	lss _stack_start,%esp
```

最后 lss 指令相当于让 ss:esp 这个栈顶指针指向了 _stack_start 这个标号的位置。还记得图里的那个原来的栈顶指针在哪里吧？往上翻一下，0x9FF00，现在要变咯。

这个 stack_start 标号定义在了很久之后才会讲到的 sched.c 里，我们这里拿出来分析一波。

```
PAGE_SIZE = 4096
long user_stack[PAGE_SIZE >> 2];

struct
{
  long *a;
  short b;
}

stack_start = {&user_stack[4096 >> 2], 0x10};
```

首先，stack_start 结构中的高位 8 字节是 0x10，将会赋值给 ss 栈段寄存器，低位 16 字节是 user_stack 这个数组的最后一个元素的地址值，将其赋值给 esp 寄存器。
 
赋值给 ss 的 0x10(0001 0000)仍然按照保护模式下的段选择子去解读，其指向的是全局描述符表中的第二个段描述符（数据段描述符），段基址是 0。

赋值给 esp 寄存器的就是 user_stack 数组的最后一个元素的内存地址值，那最终的栈顶地址，也指向了这里（user_stack + 0），后面的压栈操作，就是往这个新的栈顶地址处压咯。

![head_02](../images/03_head/head_02.jpg#pig_center)

注意，栈顶的增长方向是从高地址向低地址的。注意栈段基址和ESP在图中的位置。

```
!================== step 1, set idt, gdt ====================
	call setup_idt                         
```
设置中断描述符表，设置全局描述符表

```
/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
setup_idt:
	lea ignore_int,%edx
	movl $0x00080000,%eax
	movw %dx,%ax		/* selector = 0x0008 = cs */
	movw $0x8E00,%dx	/* interrupt gate - dpl=0, present */

	lea _idt,%edi
	mov $256,%ecx
rp_sidt:
	movl %eax,(%edi)
	movl %edx,4(%edi)
	addl $8,%edi
	dec %ecx
	jne rp_sidt
	lidt idt_descr
	ret
	...

_idt:	.fill 256,8,0		# idt is uninitialized

idt_descr:
	.word 256*8-1		# idt contains 256 entries
	.long _idt
.align 2
```

![head_03](../images/03_head/head_03.jpg#pig_center)

原理详解:

	lea ignore_int,%edx: 装载(load) ignore_int的地址到EDX寄存器中
	movl $0x00080000,%eax:设置CS寄存器的描述符表偏移EAX的值为0x00080000(0000 0000 0000 1000 0000 0000 0000 0000)
	movw %dx,%ax: 将dx中的16位移至ax中
	movw $0x8E00, %%dx: 将dx中存入值为0x8E00,表明此处设置描述符权限(DPL)等级为0，即只有特权代码才能触发中断。

	lea _idt, %edi: 获取中断描述表(IDT)开始地址
	_idt:	.fill 256,8,0: 共初始化256个实例，每个实例长度为8，初始值为0

	mov $256,%ecx: 将ECX寄存器设置为256(0x100)
	movl %eax,(%edi): 将EAX中的内容压入EDI寄存器中
	movl %edx,4(%edi): 将EDX中的内容压入EDI寄存器目前所指地址的32位之后
	addl $8,%edi: 移动指针到EDI中的下一个实例的位置
	dec %ecx: 减一
	
	lidt idt_descr: 加载IDT描述符到48位的IDTR操作寄存器中，使得CPU能够处理IDT


中断描述符表 idt 里面存储着一个个中断描述符，每一个中断号就对应着一个中断描述符，而中断描述符里面存储着主要是中断程序的地址，这样一个中断号过来后，CPU 就会自动寻找相应的中断程序，然后去执行它。
 
那这段程序的作用就是，设置了 256 个中断描述符，并且让每一个中断描述符中的中断程序例程都指向一个 ignore_int 的函数地址，这个是个默认的中断处理程序，之后会逐渐被各个具体的中断程序所覆盖。比如之后键盘模块会将自己的键盘中断处理程序，覆盖过去。
 
那现在，产生任何中断都会指向这个默认的函数 ignore_int，也就是说现在这个阶段你按键盘还不好使。

```
/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown interrupt\n\r"
.align 2
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	pushl $int_msg
	call _printk
	popl %eax
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret
```
默认中断函数，将当前的EAX，ECX，EDX，DS，ES，FS入栈，打印中断出错信息后，重新恢复现场


# 设置全局描述符表(GDT)

```
	call setup_gdt
```

```
/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
setup_gdt:
	lgdt gdt_descr
	ret

gdt_descr:
	.word 256*8-1		# so does gdt (not that that's any
	.long _gdt		# magic number, but it works for me :^)

	.align 3

_gdt:	
	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x00c09a0000000fff	/* 16Mb */
	.quad 0x00c0920000000fff	/* 16Mb */
	.quad 0x0000000000000000	/* TEMPORARY - don't use */
	.fill 252,8,0			/* space for LDT's and TSS's etc */
```

原理详解：

	_gdt：
		.quad 0x0000000000000000 /* 将开始设置为NULL 描述符 */
		.quad 0x00c09a0000fff /* 代码段设置大小为16MB(0x0FFF * 4KB),特权级别设置为内核级 */
		.quad 0x00c0920000000fff	/* 数据段设置大小为16MB(0x0FFF * 4KB),特权级别设置为内核级 */
		.quad 0x0000000000000000	/* TEMPORARY - don't use */
		.fill 252,8,0			/* space for LDT's and TSS's etc 预留252个空间，用来放置任务状态段描述符 TSS 和局部描述符 LDT/

		
![head_06](../images/03_head/head_06.jpg#pig_center)


现在，head程序要废除已有的GDT，并在内核中的新位置重新创建GDT，如图所示。其中第2项和第3项分别为内核代码段描述符和内核数据段描述符，其段限长均被设置为16 MB，并设置GDTR的值。

![head_04](../images/03_head/head_04.png#pig_center)

因为原来设置的 gdt 是在 setup 程序中，之后这个地方要被缓冲区覆盖掉，所以这里重新设置在 head 程序中，这块内存区域之后就不会被其他程序用到并且覆盖了，就这么个事。

![head_05](../images/03_head/head_05.png#pig_center)

# 检测A20总线打开


```
	movl $0x10,%eax			# reload all the segment registers
	mov %ax,%ds				# after changing gdt. CS was already
	mov %ax,%es				# reloaded in 'setup_gdt'
	mov %ax,%fs
	mov %ax,%gs
	lss _stack_start,%esp
```
现在，栈顶指针esp指向user_stack数据结构的外边缘，也就是内核栈的栈底。这样，当后面的程序需要压栈时，就可以最大限度地使用栈空间。栈顶的增长方向是从高地址向低地址的，如图所示。

![head_07](../images/03_head/head_07.jpg#pig_center)


```
	xorl %eax,%eax
1:	incl %eax				# check that A20 really IS enabled     
! ================== step 2, check A20 and math chip ====================
	movl %eax,0x000000		# loop forever if it isn't
	cmpl %eax,0x100000
	je 1b
/*
```

A20如果没打开，则计算机处于20位的寻址模式，超过0xFFFFF寻址必然“回滚”。一个特例是0x100000会回滚到0x000000，也就是说，地址0x100000处存储的值必然和地址0x000000处存储的值完全相同。通过在内存0x000000位置写入一个数据，然后比较此处和1 MB（0x100000，注意，已超过实模式寻址范围）处数据是否一致，就可以检验A20地址线是否已打开。

确定A20地址线已经打开之后，head程序如果检测到数学协处理器存在，则将其设置为保护模式工作状态，如图所示。
![head_08](../images/03_head/head_08.jpg#pig_center)

# 检测协处理器

x87协处理器：为了弥补x86系列在进行浮点运算时的不足，Intel于1980年推出了x87系列数学协处理器，那时是一个外置的、可选的芯片（笔者当时的80386计算机上就没安装80387协处理器）。1989年，Intel发布了486处理器。自从486开始，以后的CPU一般都内置了协处理器。这样，对于486以前的计算机而言，操作系统检验x87协处理器是否存在就非常必要了。

```
/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
	movl %cr0,%eax		# check math chip
	andl $0x80000011,%eax	# Save PG,PE,ET
/* "orl $0x10020,%eax" here for 486 might be good */
	orl $2,%eax		# set MP
	movl %eax,%cr0
	call check_x87
```


```
/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
	fninit
	fstsw %ax
	cmpb $0,%al
	je 1f			/* no coprocessor: have to set bits */
	movl %cr0,%eax
	xorl $6,%eax		/* reset MP, set EM */
	movl %eax,%cr0
	ret
.align 2
1:	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */
	ret
```

# 开启分页机制

```
	jmp after_page_tables   
```

```
after_page_tables:
	pushl $0		# These are the parameters to main :-)
	pushl $0
	pushl $0
	pushl $L6		# return address for main, if it decides to.
	pushl $_main
	jmp setup_paging
L6:
	jmp L6			# main should never return here, but
					# just in case, we know what happens.
```

head程序将为调用main函数做最后的准备。这是head程序执行的最后阶段，也是main函数执行前的最后阶段。
![head_18](../images/03_head/head_18.jpg#pig_center)
head程序将L6标号和main函数入口地址压栈，栈顶为main函数地址，目的是使head程序执行完后通过ret指令就可以直接执行main函数。
![head_19](../images/03_head/head_19.jpg#pig_center)


这些压栈动作完成后，head程序将跳转至setup_paging:去执行，开始创建分页机制。先要将页目录表和4个页表放在物理内存的起始位置，从内存起始位置开始的5页空间内容全部清零（每页4 KB），为初始化页目录和页表做准备。注意，这个动作起到了用1个页目录表和4个页表覆盖head程序自身所占内存空间的作用。

![head_09](../images/03_head/head_09.png#pig_center)

在没有开启分页机制时，由程序员给出的逻辑地址，需要先通过分段机制转换成物理地址。但在开启分页机制后，逻辑地址仍然要先通过分段机制进行转换，只不过转换后不再是最终的物理地址，而是线性地址，然后再通过一次分页机制转换，得到最终的物理地址。

![head_10](../images/03_head/head_10.png#pig_center)

CPU 在看到我们给出的内存地址后，首先把线性地址被拆分成

高 10 位：中间 10 位：后 12 位

高 10 位负责在页目录表中找到一个页目录项，这个页目录项的值加上中间 10 位拼接后的地址去页表中去寻找一个页表项，这个页表项的值，再加上后 12 位偏移地址，就是最终的物理地址。
 
而这一切的操作，都由计算机的一个硬件叫MMU，中文名字叫内存管理单元，有时也叫PMMU（分页内存管理单元）。由这个部件来负责将虚拟地址转换为物理地址。
 
所以整个过程我们不用操心，作为操作系统这个软件层，只需要提供好页目录表和页表即可，这种页表方案叫做二级页表，第一级叫页目录表 PDE，第二级叫页表 PTE。他们的结构如下。

![head_11](../images/03_head/head_11.png#pig_center)



```
.align 2
setup_paging:
	movl $1024*5,%ecx			/* 5 pages - pg_dir+4 page tables */
	xorl %eax,%eax
	xorl %edi,%edi				/* pg_dir is at 0x000 */
	cld;rep;stosl
	movl $pg0+7,_pg_dir			/* set present bit/user r/w */
	movl $pg1+7,_pg_dir+4		/*  --------- " " --------- */
	movl $pg2+7,_pg_dir+8		/*  --------- " " --------- */
	movl $pg3+7,_pg_dir+12		/*  --------- " " --------- */
	movl $pg3+4092,%edi
	movl $0xfff007,%eax			/*  16Mb - 4096 + 7 (r/w user,p) */
	std
1:	stosl						/* fill pages backwards - more efficient :-) */
	subl $0x1000,%eax
	jge 1b
```

所以这段代码，就是帮我们把页表和页目录表在内存中写好，之后开启 cr0 寄存器的分页开关，仅此而已，我们再把代码贴上来。

原理详解:

	movl $1024*5, %ecx: 将ECX设置为1024*5
	xorl %eax, %eax: 将EAX寄存器清零
	xorl %edi, %edi: 将EDI寄存器清零
	cld:	设置写顺序，确保每次pg_dir增加
	rep, stos1: 往EDI所指的位置，重复写入1024*5次

	movl $pg0+7, _pg_dir: 将_pg_dir设置为0x1007(0001 0000 0000 0111),即设置页表0起始地址为0x1000，用户有读写权限，存在页面。

	movl $pg3+4092,%edi: EDI中存储0x4ffc(0x4000 + 4092(0xffc)) 
	movl $0xfff007,%eax: 0x007设置用户读写权限且可执行，页面存在；0xfff表示将前20位设置为0x0fff(0000 1111 1111 1111)物理地址,即16MB页表的最后一页，即将EAX寄存器设置为16MB-4k+7(r/w user, p)
	std: 设置方向direction bit位为1(相反),开始逆序
1:	stosl: 将EAX中值存入EDI中所指的位置
	subl $0x1000, %eax: EAX减去0x1000
	jge 1b: 循环判断，当EAX寄存器不大于0停止循环，将16MB内存地址全部设置权限为(r/w user, p)

继续设置页表。将第4个页表（由pg3指向的位置）的倒数第二个页表项（pg3-4+ 4902指向的位置）指向倒数第二个页面，即0xFFF000～0x1000（0x1000即4KB，一个页面的大小）开始的4 KB字节内存空间。请读者认真对比下述两图，有多处位置发生了变化。

![head_13](../images/03_head/head_13.jpg#pig_center)

![head_14](../images/03_head/head_14.jpg#pig_center)

```
	xorl %eax,%eax				/* pg_dir is at 0x0000 */
	movl %eax,%cr3				/* cr3 - page directory start */
	movl %cr0,%eax
	orl $0x80000000,%eax
	movl %eax,%cr0				/* set paging (PG) bit */
```

之后再开启分页机制的开关。其实就是更改 cr0 寄存器中的一位即可（31 位），还记得我们开启保护模式么，也是改这个寄存器中的一位的值。

原理详解：

	movl %eax, %cr3: 将EAX寄存器中的值移入CR3寄存器中
	movl %cr0, %eax: 将CR0寄存器中的值移入EAX中
	orl  $0x80000000, %eax: 使能分页(PG: Paging enable)位
	movl %eax, %cr0: 将使能的结果设置到CR0寄存器中

![head_12](../images/03_head/head_12.png#pig_center)

然后，MMU 就可以帮我们进行分页的转换了。此后指令中的内存地址（就是程序员提供的逻辑地址），就统统要先经过分段机制的转换，再通过分页机制的转换，才能最终变成物理地址。

当时 linux-0.11 认为，总共可以使用的内存不会超过 16M，也即最大地址空间为 0xFFFFFF。
 
而按照当前的页目录表和页表这种机制，1 个页目录表最多包含 1024 个页目录项（也就是 1024 个页表），1 个页表最多包含 1024 个页表项（也就是 1024 个页），1 页为 4KB（因为有 12 位偏移地址），因此，16M 的地址空间可以用 1 个页目录表 + 4 个页表搞定。
 
4（页表数）* 1024（页表项数） * 4KB（一页大小）= 16MB
 
所以，上面这段代码就是，将页目录表放在内存地址的最开头，还记得上一讲开头让你留意的 _pg_dir 这个标签吧？

```
_pg_dir:
_startup_32:
    mov eax,0x10
    mov ds,ax
    ...
```

之后紧挨着这个页目录表，放置 4 个页表，代码里也有这四个页表的标签项。

```
.org 0x1000 pg0:
.org 0x2000 pg1:
.org 0x3000 pg2:
.org 0x4000 pg3:
.org 0x5000
```

这些工作完成后，内存中的布局如图1-43所示。可以看出，只有184字节的剩余代码。由此可见，在设计head程序和system模块时，其计算是非常精确的，对head.s的代码量的控制非常到位。

![head_15](../images/03_head/head_15.jpg#pig_center)

![head_16](../images/03_head/head_16.png#pig_center)

# 调用主函数

```
	ret							/* this also flushes prefetch-queue */
```

head程序执行最后一步：ret。这要通过跳入main函数程序执行。在图1-36中，main函数的入口地址被压入了栈顶。现在执行ret了，正好将压入的main函数的执行入口地址弹出给EIP。标示了出栈动作。

![head_17](../images/03_head/head_17.jpg#pig_center)


这部分代码用了底层代码才会使用的技巧。我们结合上图对这个技巧进行详细讲解。我们先看看普通函数的调用和返回的方法。因为Linux 0.11用返回方法调用main函数，返回位置和main函数的入口在同一段内，所示我们只讲解段内调用和返回，如图所示

![head_20](../images/03_head/head_20.jpg#pig_center)

call指令会将EIP的值自动压栈，保护返回现场，然后执行被调函数的程序。等到执行被调函数的ret指令时，自动出栈给EIP并还原现场，继续执行call的下一行指令。这是通常的函数调用方法。对操作系统的main函数来说，这个方法就有些怪异了。main函数是操作系统的。如果用call调用操作系统的main函数，那么ret时返回给谁呢？难道还有一个更底层的系统程序接收操作系统的返回吗？操作系统已经是最底层的系统了，所以逻辑上不成立。那么如何既调用了操作系统的main函数，又不需要返回呢？操作系统的设计者采用了上图所示的方法。

这个方法的妙处在于，是用ret实现的调用操作系统的main函数。既然是ret调用，当然就不需要再用ret了。不过，call做的压栈和跳转的动作谁来做呢？操作系统的设计者做了一个仿call的动作，手工编写代码压栈和跳转，模仿了call的全部动作，实现了调用setup_paging函数。注意，压栈的EIP值并不是调用setup_paging函数的下一行指令的地址，而是操作系统的main函数的执行入口地址_main。这样，当setup_paging函数执行到ret时，从栈中将操作系统的main函数的执行入口地址_main自动出栈给EIP，EIP指向main函数的入口地址，实现了用返回指令“调用”main函数。

在下图中，将压入的main函数的执行入口地址弹出给CS:EIP，这句话等价于CPU开始执行main函数程序。标示了这个状态：

![head_21](../images/03_head/head_21.jpg#pig_center)

	为什么没有最先调用main函数？
	
	学过C语言的人都知道，用C语言设计的程序都有一个main函数，而且是从main函数开始执行的。Linux 0.11的代码是用C语言编写的。奇怪的是，为什么在操作系统启动时先执行的是三个由汇编语言写成的程序，然后才开始执行main函数；为什么不是像我们熟知的C语言程序那样，从main函数开始执行呢。
		
	通常，我们用C语言编写的程序都是用户应用程序。这类程序的执行有一个重要的特征，就是必须在操作系统的平台上执行，也就是说，要由操作系统为应用程序创建进程，并把应用程序的可执行代码从硬盘加载到内存。现在我们讨论的是操作系统，不是普通的应用程序，这样就出现了一个问题：应用程序是由操作系统加载的，操作系统该由谁加载呢？

	从前面的节中我们知道，加载操作系统的时候，计算机刚刚加电，只有BIOS程序在运行，而且此时计算机处在16位实模式状态，通过BIOS程序自身的代码形成的16位的中断向量表及相关的16位的中断服务程序，将操作系统在软盘上的第一扇区（512字节）的代码加载到内存，BIOS能主动操作的内容也就到此为止了。准确地说，这是一个约定。对于第一扇区代码的加载，不论是什么操作系统都是一样的；从第二扇区开始，就要由第一扇区中的代码来完成后续的代码加载工作。当加载工作完成后，好像仍然没有立即执行main函数，而是打开A20，打开pe、pg，建立IDT、GDT……然后才开始执行main函数，这是什么道理？

	原因是，Linux 0.11是一个32位的实时多任务的现代操作系统，main函数肯定要执行的是32位的代码。编译操作系统代码时，是有16位和32位不同的编译选项的。如果选了16位，C语言编译出来的代码是16位模式的，结果可能是一个int型变量，只有2字节，而不是32位的4字节……这不是Linux 0.11想要的。Linux 0.11要的是32位的编译结果。只有这样才能成为32位的操作系统代码。这样的代码才能用到32位总线（打开A20后的总线），才能用到保护模式和分页，才能成为32位的实时多任务的现代操作系统。
	
	开机时的16位实模式与main函数执行需要的32位保护模式之间有很大的差距，这个差距谁来填补？ head.s做的就是这项工作。这期间，head程序打开A20，打开pe、pg，废弃旧的、16位的中断响应机制，建立新的32位的IDT……这些工作都做完了，计算机已经处在32位的保护模式状态了，调用32位main函数的一切条件已经准备完毕，这时顺理成章地调用main函数。后面的操作就可以用32位编译的main函数完成。

至此，Linux 0.11内核启动的一个重要阶段已经完成，接下来就要进入main函数对应的代码了。

特别需要提示的是，**此时仍处在关闭中断的状态！**

# 地址再探
 
逻辑地址：我们程序员写代码时给出的地址叫逻辑地址，其中包含段选择子和偏移地址两部分。
 
线性地址：通过分段机制，将逻辑地址转换后的地址，叫做线性地址。而这个线性地址是有个范围的，这个范围就叫做线性地址空间，32 位模式下，线性地址空间就是 4G。
 
物理地址：就是真正在内存中的地址，它也是有范围的，叫做物理地址空间。那这个范围的大小，就取决于你的内存有多大了。
 
虚拟地址：如果没有开启分页机制，那么线性地址就和物理地址是一一对应的，可以理解为相等。如果开启了分页机制，那么线性地址将被视为虚拟地址，这个虚拟地址将会通过分页机制的转换，最终转换成物理地址

逻辑地址是程序员给出的，经过分段机制转换后变成线性地址，然后再经过分页机制转换后变成物理地址，就这么简单。

我们通过bootsect,setup,以及head部分完成了

![head_22](../images/03_head/head_22.png#pig_center)

现在的内存布局如下所示：

![head_23](../images/03_head/head_23.png#pig_center)
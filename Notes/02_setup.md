# 
本文解析setup.s，主要可以分为部分：
- 第一部分为进入setup后，提取机器系统数据(-110)
- 

```
INITSEG  = 0x9000	! we move boot here - out of the way
SYSSEG   = 0x1000	! system loaded at 0x10000 (65536).
SETUPSEG = 0x9020	! this is the current segment

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

.text
endtext:
.data
enddata:
.bss
endbss:
```

数据初始化

```
	mov	ax,#INITSEG	    ! this is done in bootsect already, but...
	mov	ds,ax
	mov	ah,#0x03	    ! read cursor pos
	xor	bh,bh
	int	0x10		    ! save it in known place, con_init fetches
	mov	[0],dx		    ! it from 0x90000.
```

读取光标位置，并将光标位置存储在ds内存储的0x9000+[0]处，即0x90000

```
! Get memory size (extended mem, kB) 

	mov	ah,#0x88
	int	0x15
	mov	[2],ax

```
获取内存信息

```
! Get video-card data:

	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width
```

获取显卡信息

```
! check for EGA/VGA and some config parameters

	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax
	mov	[10],bx
	mov	[12],cx
```

获取EGA/VGA和配置参数(检查显示方式并取参数)

```
! Get hd0 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080
	mov	cx,#0x10
	rep
	movsb
```

获取首磁盘信息,从ds(0x0000):si(0x0104)拷贝到es(0x9000):di(0x0080),重复16次

![setup_01](../images/02_setup/setup_01.png#pic_center)

```
! Get hd1 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x46]
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	rep
	movsb
```

获取第二块磁盘信息,从ds(0x0000):si(0x0118)拷贝到es(0x9000):di(0x0090),重复16次

从设备获取及存储的信息如下：

| 内存地址 | 长度 | 名称 |
| :-----:| :----: | :----: |
| 0x90000 | 2 | 光标位置 |
| 0x90002 | 2 | 扩展内存数 |
| 0x90004 | 2 | 显示页面 |
| 0x90006 | 1 | 显示模式 |
| 0x90007 | 1 | 字符列数 |
| 0x90008 | 2 | 未知2 |
| 0x9000A | 1 | 显示内存 |
| 0x9000B | 1 | 显示状态 |
| 0x9000C | 2 | 显卡特性参数 |
| 0x9000E | 1 | 屏幕行数 |
| 0x9000F | 1 | 屏幕列数 |
| 0x90080 | 16 | 硬盘1参数列表 |
| 0x90090 | 16 | 硬盘2参数列表 |
| 0x901FC | 2 | 根设备号 |

```
! Check that there IS a hd1 :-)

	mov	ax,#0x01500
	mov	dl,#0x81
	int	0x13
	jc	no_disk1
	cmp	ah,#3
	je	is_disk1
no_disk1:
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	mov	ax,#0x00
	rep
	stosb
is_disk1:

! now we want to move to protected mode ...

	cli			    ! no interrupts allowed !
```

磁盘检查，没有磁盘就重新加载，有的话，就cli关闭(close)中断，准备进入保护模式,这个准备工作先要关闭中断，即将CPU的标志寄存器（EFLAGS）中的中断允许标志（IF）置0。这意味着，程序在接下来的执行过程中，无论是否发生中断，系统都不再对此中断进行响应。


| 31 | 30 | 29 |28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|9|8|7|6|5|4|3|2|1|0|
| :----: | :----: | :-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |:-----:| :----: | :----: |
| 0 | 0 | 0|0| 0| 0| 0|0 |0| 0 | 0 | ID |VIP| VIF | AC | VM |RF| 0 |NT | IOPL |OF| DF | IF | TF |SF| ZF | 0| AF |0| PF | 1 | CF |


```
! first we move the system to it's rightful place

	mov	ax,#0x0000
	cld			        ! 'direction'=0, movs moves forward
do_move:                !========================= step 2: move system to 0x00000 ==============
	mov	es,ax		    ! destination segment
	add	ax,#0x1000
	cmp	ax,#0x9000
	jz	end_move
	mov	ds,ax		    ! source segment
	sub	di,di
	sub	si,si
	mov cx,#0x8000
	rep
	movsw
	jmp	do_move

    ...
```

将内存中的从0x1000基址开始的内存都搬到以0x0000开始的位置，直到ax中的值变为0x9000

![setup_02](../images/02_setup/setup_02.png#pic_center)

栈顶地址仍然是 0x9FF00 没有改变。
 
0x90000 开始往上的位置，原来是 bootsect 和 setup 程序的代码，现 bootsect 的一部分代码在已经被操作系统为了记录内存、硬盘、显卡等一些临时存放的数据给覆盖了一部分。
 
内存最开始的 0 到 0x80000 这 512K 被 system 模块给占用了，之前讲过，这个 system 模块就是除了 bootsect 和 setup 之外的全部程序链接在一起的结果，可以理解为操作系统的全部。

那么现在的内存布局就是这个样子。

![setup_03](../images/02_setup/setup_03.png#pic_center)

```
end_move:                   !========================= step 3: load idt and gdt ==============
	mov	ax,#SETUPSEG        ! right, forgot this at first. didn't work :-)
	mov	ds,ax
	lidt	idt_48		    ! load idt with 0,0
	lgdt	gdt_48		    ! load gdt with 0x200+gdt, 0x800

```

加载idt，gdt的地址到IDTR，GDTR。

![setup_04](../images/02_setup/setup_04.jpg#pig_center)


```
idt_48:
	.word	0			    ! idt limit=0
	.word	0,0			    ! idt base=0L       !把地址与限长加载到IDTR0|0|0

gdt_48:
	.word	0x800		    ! gdt limit=2048, 256 GDT entries
	.word	512+gdt,0x9	    ! gdt base = 0X9|200+gdt|0x800 把地址与限长加载到GDTR

gdt:
	.word	0,0,0,0		    ! dummy              ! 第 1 个描述符,不用

	.word	0x07FF		    ! 8Mb - limit=2047 (2048*4096=8Mb) !第 2 个, for code
	.word	0x0000		    ! base address=0
	.word	0x9A00		    ! code read/exec
	.word	0x00C0		    ! granularity=4096, 386

	.word	0x07FF		    ! 8Mb - limit=2047 (2048*4096=8Mb) !第 3 个, for data
	.word	0x0000		    ! base address=0
	.word	0x9200		    ! data read/write
	.word	0x00C0		    ! granularity=4096, 386
```

IDT虽然已经设置，实为一张空表，原因是目前已关中断，无需调用中断服务程序。此处反映的是数据“够用即得”的思想。

原理详解:

    **0,0,0,0**: GDT从1开始，所以需要先输入第0个描述符的内容；
    **0x07FF**:  表示2048页的4096 bytes 的数据，即8MB的段大小的限制
    **0x0000**:  表示代码段的基址
    **0x9A00**:  表示段属性，"9"表示代码段，"A"表示可执行,"00"表示访问权限都在level0级别
    **0x00C0**:  表示页的大小为4KB,处理模式为386下，该段能够被访问

    **0x9200**:  表示可读写，但不可执行

![setup_05](../images/02_setup/setup_05.png#pig_center)

因为，此时此刻内核尚未真正运行起来，还没有进程，所以现在创建的GDT第一项为空，第二项为内核代码段描述符，第三项为内核数据段描述符，其余项皆为空。



创建这两个表的过程可理解为是分两步进行的：

- 1. 在设计内核代码时，已经将两个表写好，并且把需要的数据也写好。
- 2. 将专用寄存器（IDTR、GDTR）指向表。

![setup_06](../images/02_setup/setup_06.png#pig_center)

原理详解：

    GDT（Global Descriptor Table，全局描述符表），在系统中唯一的存放段寄存器内容（段描述符）的数组，配合程序进行保护模式下的段寻址。它在操作系统的进程切换中具有重要意义，可理解为所有进程的总目录表，其中存放每一个任务（task）局部描述符表（LDT，Local Descriptor Table）地址和任务状态段（TSS，Task Structure Segment）地址，完成进程中各段的寻址、现场保护与现场恢复。

    GDTR（Global Descriptor Table Register，GDT基地址寄存器），GDT可以存放在内存的任何位置。当程序通过段寄存器引用一个段描述符时，需要取得GDT的入口，GDTR标识的即为此入口。在操作系统对GDT的初始化完成后，可以用LGDT（Load GDT）指令将GDT基地址加载至GDTR。

    IDT（Interrupt Descriptor Table，中断描述符表），保存保护模式下所有中断服务程序的入口地址，类似于实模式下的中断向量表。

    IDTR（Interrupt Descriptor Table Register，IDT基地址寄存器），保存IDT的起始地址。

    32位的中断机制和16位的中断机制，在原理上有比较大的差别。最明显的是16位的中断机制用的是中断向量表，中断向量表的起始位置在0x00000处，这个位置是固定的；32位的中断机制用的是中断描述符表（IDT），位置是不固定的，可以由操作系统的设计者根据设计要求灵活安排，由IDTR来锁定其位置。

    GDT是保护模式下管理段描述符的数据结构，对操作系统自身的运行以及管理、调度进程有重大意义，后面的章节会有详细讲解。

    值得一提的是，在内存中做出数据的方法有两种：
        1）划分一块内存区域并初始化数据，“看住”这块内存区域，使之能被找到；
        2）由代码做出数据，如用push代码压栈，“做出”数据。
    
    此处采用的是第一种方法。


```
! that was painless, now we enable A20
!========================= step 4: open A20 ==============
	call	empty_8042
	mov	al,#0xD1		    ! command write
	out	#0x64,al
	call	empty_8042
	mov	al,#0xDF		    ! A20 on
	out	#0x60,al
	call	empty_8042
    ...
```
清空内存后，写入初始化控制字后，打开A20，实现32位寻址

```
empty_8042:
	.word	0x00eb,0x00eb
	in	al,#0x64	        ! 8042 status port
	test	al,#2		    ! is input buffer full?
	jnz	empty_8042	        ! yes - loop
	ret
```

empty_8042表示循环清空8042端口


```
! well, that went ok, I hope. Now we have to reprogram the interrupts :-(
! we put them right after the intel-reserved hardware interrupts, at
! int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
! messed this up with the original PC, and they haven't been able to
! rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
! which is used for the internal hardware interrupts as well. We just
! have to reprogram the 8259's, and it isn't fun.
!========================= step 5: reprogram int of 8259 ==============
	mov	al,#0x11		        ! initialization sequence
	out	#0x20,al		        ! send it to 8259A-1
	.word	0x00eb,0x00eb	    ! jmp $+2, jmp $+2
	out	#0xA0,al		        ! and to 8259A-2
	.word	0x00eb,0x00eb
```
开始重编程8259A,首先将初始化序列指令发送给8259A-1的控制器，等待两个循环后，再发送给8259A-2控制器

```
	mov	al,#0x20		        ! start of hardware int's (0x20)
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x28		        ! start of hardware int's 2 (0x28)
	out	#0xA1,al
	.word	0x00eb,0x00eb
```
分配设置控制器的时钟中断

```
	mov	al,#0x04		        ! 8259-1 is master
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x02		        ! 8259-2 is slave
	out	#0xA1,al
	.word	0x00eb,0x00eb
```
设置8259A-1为主控制器，8259A-2为从控制器


```
	mov	al,#0x01		        ! 8086 mode for both
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al
	.word	0x00eb,0x00eb
```
设置主从控制器均为8086模式


```
	mov	al,#0xFF		        ! mask off all interrupts for now
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al

! well, that certainly wasn't fun :-(. Hopefully it works, and we don't
! need no steenking BIOS anyway (except for the initial loading :-).
! The BIOS-routine wants lots of unnecessary data, and it's less
! "interesting" anyway. This is how REAL programmers do it.
```
在初始化进程中，暂时去使能所有硬件中断。

这里是对可编程中断控制器 8259 芯片进行的编程。
 
因为中断号是不能冲突的， Intel 把 0 到 0x19 号中断都作为保留中断，比如 0 号中断就规定为除零异常，软件自定义的中断都应该放在这之后，但是 IBM 在原 PC 机中搞砸了，跟保留中断号发生了冲突，以后也没有纠正过来，所以我们得重新对其进行编程，不得不做，却又一点意思也没有。这是 Linus 在上面注释上的原话。

所以我们也不必在意，只要知道重新编程之后，8259 这个芯片的引脚与中断号的对应关系，变成了如下的样子就好。


| PIC 请求号| 中断号 | 用途 |
| :-----:| :----: | :----: |		
|IRQ0	|0x20|	时钟中断|
|IRQ1	|0x21|	键盘中断|
|IRQ2	|0x22|	接连从芯片|
|IRQ3	|0x23|	串口2|
|IRQ4	|0x24|	串口1|
|IRQ5	|0x25|	并口2|
|IRQ6	|0x26|	软盘驱动器|
|IRQ7	|0x27|	并口1|
|IRQ8	|0x28|	实时钟中断|
|IRQ9	|0x29|	保留|
|IRQ10	|0x2a|	保留|
|IRQ11	|0x2b|	保留|
|IRQ12	|0x2c|	鼠标中断|
|IRQ13	|0x2d|	数学协处理器|
|IRQ14	|0x2e|	硬盘中断|
|IRQ15	|0x2f|	保留|

```
! Well, now's the time to actually move into protected mode. To make
! things as simple as possible, we do no register set-up or anything,
! we let the gnu-compiled 32-bit programs do that. We just jump to
! absolute address 0x00000, in 32-bit protected mode.

	mov	ax,#0x0001	            ! protected mode (PE) bit
	lmsw	ax		            ! This is it!
```
从实模式进入保护模式。

![setup_07](../images/02_setup/setup_07.png#pig_center)
原理详解:

    lmsw: load machine status word, 加载ax中数值到MSW寄存器，执行后，系统将从实模式进入保护模式。

```
    jmpi	0,8		            ! jmp offset 0 of segment 8 (cs)
```

以偏移0使用段选择符8(0x1000)进入GDT中第二项,即代码段


原理详解：

    jmpi 0,8: 这一行代码中的“0”是段内偏移，“8”是保护模式下的段选择符，用于选择描述符表和描述符表项以及所要求的特权级。这里“8”的解读方式很有意思。如果把“8”当做6、7、8……中的“8”这个数来看待，这行程序的意思就很难理解了。必须把“8”看成二进制的1000，再把前后相关的代码联合起来当做一个整体看，在头脑中形成类似图1-23所示的图，才能真正明白这行代码究竟在说什么。注意：这是一个以位为操作单位的数据使用方式，4 bit的每一位都有明确的意义，这是底层源代码的一个特点。
    
    这里1000的最后两位（00）表示内核特权级，与之相对的用户特权级是11；第三位的0表示GDT，如果是1，则表示LDT；1000的1表示所选的表（在此就是GDT）的1项（GDT项号排序为0项、1项、2项，这里也就是第2项）来确定代码段的段基址和段限长等信息。从图1-23中我们可以看到，代码是从段基址0x00000000、偏移为0处，也就是head程序的开始位置开始执行的，这意味着执行head程序。

![setup_09](../images/02_setup/setup_09.jpg#pig_center)
![setup_10](../images/02_setup/setup_10.jpg#pig_center)


对照段选择子的结构，可以知道描述符索引值是 1，也就是要去全局描述符表（gdt）中找第一项段描述符。

![setup_08](../images/02_setup/setup_08.png#pig_center)

我们说了，第 0 项是空值，第一项被表示为代码段描述符，是个可读可执行的段，第二项为数据段描述符，是个可读可写段，不过他们的段基址都是 0。
 
所以，这里取的就是这个代码段描述符，段基址是 0，偏移也是 0，那加一块就还是 0 咯，所以最终这个跳转指令，就是跳转到内存地址的 0 地址处，开始执行。

到这里为止，setup就执行完毕了，它为系统能够在保护模式下运行做了一系列的准备工作。但这些准备工作还不够，后续的准备工作将由head程序来完成。


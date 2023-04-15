本节从创建进程1开始


# 进程0创建进程1

```
    if (!fork()) {
        init();
    }

    for(;;) pause();
}
```

在Linux操作系统中创建新进程的时候，都是由父进程调用fork函数来实现的。该过程如图

![processOne_08](../images/05_processOne/processOne_08.jpg#pig_center)

## 进程0创建进程1

```

#define __NR_fork	2

#define _syscall0(type,name) \
type name(void) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \     // int 0x80是所有系统调用函数的总入口
	: "=a" (__res) \                // 输出部分，将__res值赋给eax
	: "0" (__NR_##name)); \         // 输入部分，"0",同上寄存器，即eax
if (__res >= 0) \                   // int 0x80中断返回后，执行本行
	return (type) __res; \
errno = -__res; \
return -1; \
}

// 复制进程
int fork(void);

static inline _syscall0(int,fork)                  //inline is like #define, but expande in compile, and more flexible with variables

```

**__res**: 定义了long型的变量__res来接收system call的返回值

**volatile**: 告诉编译器，由于这些指令对系统可能产生影响，因此需要禁止针对这部分代码的优化，同时可以让汇编语言所描述的功能可以被程序正确地执行。

**__NR_##name**: 在Linux内核中，系统调用是通过使用汇编指令INT 0x80来实现的，它会使得CPU切换到内核态并执行与指定的系统调用号相对应的代码。因此，调用一个系统调用需要以某种特定的方式将调用号和其他参数传递给内核，而这里使用EAX寄存器来保存系统调用号，而其他寄存器则根据其在系统调用中的参数次序进行了选择。

**__res >= 0** : 判断返回结果正确


紧接着就执行"int $0x80"，产生一个软中断，CUP从3特权级的进程0代码跳到0特权级内核代码中执行。中断使CPU硬件自动将**SS、ESP、EFLAGS、CS、EIP这5个寄存器的数值按照这个顺序压入如图所示的init_task中的进程0内核栈**。注意其中init_task结构后面的红条，表示了刚刚压入内核栈的寄存器数值。前面刚刚提到的move_to_user_mode这个函数中做的压栈动作就是模仿中断的硬件压栈，这些压栈的数据将在后续的copy_process()函数中用来初始化进程1的TSS。


详细的执行步骤如下：

```
// 系统调用函数指针表。用于系统调用中断处理程序(int 0x80)，作为跳转表
fn_ptr sys_call_table[] = { sys_setup, sys_exit, **sys_fork**, sys_read,sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
sys_setreuid,sys_setregid };

```

先执行: "0"（__NR_ fork）这一行，意思是将fork在sys_call_table[]中对应的函数编号__NR_ fork（也就是2）赋值给eax。这个编号即sys_fork()函数在sys_call_table中的偏移值。

```
SIG_CHLD	= 17

EAX		= 0x00
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28
OLDSS		= 0x2C

state	= 0		# these are offsets into the task-struct.
counter	= 4
priority = 8
signal	= 12
sigaction = 16		# MUST be 16 (=len of sigaction)
blocked = (33*16)

# offsets within sigaction
sa_handler = 0
sa_mask = 4
sa_flags = 8
sa_restorer = 12



nr_system_calls = 72

.globl _system_call,_sys_fork,_timer_interrupt,_sys_execve
.globl _hd_interrupt,_floppy_interrupt,_parallel_interrupt
.globl _device_not_available, _coprocessor_error

.align 2
bad_sys_call:
	movl $-1,%eax
	iret
.align 2
reschedule:
	pushl $ret_from_sys_call
	jmp _schedule
.align 2
    _system_call:
        /* 比较eax中指令是否是合法的系统调用指令，如不合法，调用bad_sys_call */
        cmpl $nr_system_calls-1,%eax
        ja bad_sys_call
        /* 将DS，ES，FS现值入栈，下面6个push是为了copy_process()的参数 */
        push %ds
        push %es
        push %fs
        /* 把EDX, ECX，EBX值入栈 */
        pushl %edx
        pushl %ecx		# push %ebx,%ecx,%edx as parameters
        pushl %ebx		# to the system call
        /* 将DS，ES设置为0x10*/
        movl $0x10,%edx		# set up ds,es to kernel space
        mov %dx,%ds
        mov %dx,%es
        /* 将FS 设置为0x17*/
        movl $0x17,%edx		# fs points to local data space
        mov %dx,%fs
        /* 可以看成是call(_sys_call_table + %eax<<4), 即system_call 
         * 对应的入口, call _sys_call_table（,%eax,4）指令本身也会压栈保护现场，
         * 这个压栈体现在后面copy_process函数的第6个参数long none
        */ 
        call _sys_call_table(,%eax,4)


```

## 在task[64]中为进程1 申请一个空闲位置并获取进程号

```
/*为新进程取得不重复的进程号 last_pid，并返回在任务数组中的任务号(数组 index)  一直
* 寻找，无退路
*/
long last_pid=0;

#define EAGAIN		11    // 资源暂时不可用， try again

int find_empty_process(void) 
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}


.align 2
_sys_fork:
	call _find_empty_process        // 调用find_empty_process
	testl %eax,%eax                 // 如果返回的结果是-EAGAIN(-11)说明已有64个进程在运行
	js 1f
    /* 5个push也作为copy_process的入参 */
	push %gs                        
	pushl %esi
	pushl %edi
	pushl %ebp
    /* 最后压栈的eax的值就是find_empty_process()函数返回的任务号，也将是copy_process()函数的第一个参数int nr */
	pushl %eax
	call _copy_process
```

## 调用copy_process函数

进程0已经成为一个可以创建子进程的父进程，在内核中有“进程0的task_struct”和“进程0的页表项”等专属进程0的管理信息。进程0将在copy_process()函数中做非常重要的、体现父子进程创建机制的工作：

1. 为进程1创建task_struct，将进程0的task_struct的内容复制给进程1。
2. 为进程1的task_struct、tss做个性化设置。
3. 为进程1创建第一个页表，将进程0的页表项内容赋给这个页表。
4. 进程1共享进程0的文件。
5. 设置进程1的GDT项。
6. 最后将进程1设置为就绪态，使其可以参与进程间的轮转。
   
现在调用copy_process()函数！在讲解copy_process()函数之前，值得提醒的是，所有的参数都是前面的代码累积压栈形成的，这些参数的数值都与压栈时的状态有关。执行代码如下

```

/* these are not to be changed without changing head.s etc */
#define LOW_MEM 0x100000							// 内存低端（1M）
#define PAGING_MEMORY (15*1024*1024)				// 分页内存 15M
#define PAGING_PAGES (PAGING_MEMORY>>12)            // 0xf00 = 3840  分页后的页数

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
/*  取空闲页面。如果已经没有内存了，则返回 0。 
 *  输入：%1(ax=0) - 0；%2(LOW_MEM)；%3(cx=PAGING PAGES)；%4(di=mem_map+PAGING_PAGES-1)。 
 *  输出：返回%0(ax=页面号)。 
 *  从内存映像末端开始向前扫描所有页面标志（页面总数为 PAGING_PAGES），如果有页面空闲（对应 
 *  内存映像位为 0）则返回页面地址。
*/
unsigned long get_free_page(void)               //遍历mem map[],找到主内存中(从高地址开始)第一个空闲页面
{

/* 这一行代码中的 __res 被声明为寄存器变量，其分配在ax寄存器上。这意味着，
 * 函数在返回时可能会将一个值放置到 ax 寄存器中，然后弹出栈上的该变量，
 * 以此实现更有效地传递函数返回值的目的。
*/
register unsigned long __res asm("ax");

    /* 反向扫描串(mem map[]),al(0)与di不相等则
     * 重复(找引用对数为0的项目)
    */
__asm__("std ; repne ; scasb\n\t"               
	"jne 1f\n\t"                                // 找不到空闲页，则跳转1      
    /* 将1赋值给EDI+1,在mem map[]中
     * 将找到的0的项的引用计数设置为1
    */      
	"movb $1,1(%%edi)\n\t"                       
	"sall $12,%%ecx\n\t"                        // ECX左移12位，得到页的相对地址
	"addl %2,%%ecx\n\t"                         // LOW_MEM + ECX，得到页的物理地址
	"movl %%ecx,%%edx\n\t"                      // 将ECX移入EDX
	"movl $1024,%%ecx\n\t"                      // 将1024赋值给ECX
	"leal 4092(%%edx),%%edi\n\t"                // 将EDX+4092的地址赋值给EDI
	"rep ; stosl\n\t"                           // 初始化清空得到的页面
	"movl %%edx,%%eax\n"
	"1:"
	:"=a" (__res)
	:"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
	"D" (mem_map+PAGING_PAGES-1)                // EDX，mem map[]的最后一个元素
	:"di","cx","dx");                           // 第三个冒号是程序中改变过的值
return __res;
}
```

进入copy_process()函数后，调用get_free_page()函数，在主内存申请一个空闲页面，并将申请到的页面清零，用于进程1的task_struct及内核栈。按照get_free_page()函数的算法，是从主内存的末端开始向低地址端递进，现在是开机以来，操作系统内核第一次为进程在主内存申请空闲页面，申请到的空闲页面肯定在16 MB主内存的最末端。


```
#define TASK_UNINTERRUPTIBLE	2	// 进程处于不可中断等待状态，主要用于 I/O 操作等待

struct file { 
	unsigned short f_mode;      // 文件操作模式（RW 位）    
	unsigned short f_flags;     // 文件打开和控制的标志
	unsigned short f_count;     // 对应文件句柄（文件描述符）数
	struct m_inode * f_inode;   // 指向对应 i 节点
	off_t f_pos;                // 文件位置（读写偏移值）
};


/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
/* 
 * OK，下面是主要的 fork 子程序。它复制系统进程信息(task[n])并且设置必要的寄存器。 
 * 它还整个地复制数据段。 
 */ 
/* 复制进程。  复制task_struct（包含tss和ldt等），复制寄存器，复制内存；再进行一些个性化设置（如各种 id，状态等）
*/
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;

	p = (struct task_struct *) get_free_page();
	if (!p)
		return -EAGAIN;
```

C语言中的指针有地址的含义，更有类型的含义！强制类型转换的意思是“认定”这个页面的低地址端就是进程1的task_struct的首地址，同时暗示了高地址部分是内核栈。了解了这一点，后面的p->tss.esp0 = PAGE_SIZE +（long）p就不奇怪了。

task_struct是操作系统标识、管理进程的最重要的数据结构，每一个进程必须具备只属于自己的、唯一的task_struct。

![processOne_01](../images/05_processOne/processOne_01.jpg#pig_center)

task_union的设计颇具匠心。前面是task_struct，后面是内核栈，增长的方向正好相反，正好占用一页，顺应分页机制，分配内存非常方便。而且操作系统设计者肯定经过反复测试，保证内核代码所有可能的调用导致压栈的最大长度都不会覆盖前面的task_struct。因为内核代码都是操作系统设计者设计的，可以做到心中有数。相反，假如这个方法为用户进程提供栈空间，恐怕要出大问题了。

```
	task[nr] = p; 
```

回到copy_process函数，将这个页面的指针强制类型转换为指向task_struct的指针类型，并挂接在task[1]上，即task[nr] = p。nr就是第一个参数，是find_empty_process函数返回的任务号。

```
    /* NOTE! this doesn't copy the supervisor stack */     
    /* 完全复制之前的进程的 task_struct */
	*p = *current;	  
```

current是指向当前进程的指针；p是进程1的指针。当前进程是进程0，是进程1的父进程。将父进程的task_struct复制给子进程，就是将父进程最重要的进程属性复制给了子进程，子进程继承了父进程的绝大部分能力。这是父子进程创建机制的特点之一。

进程1的task_struct的雏形此时已经形成了，进程0的task_struct中的信息并不一定全都适用于进程1，因此还需要针对具体情况进行调整。初步设置进程1的task_struct如图3-6所示。从p->开始的代码，都是为进程1所做的个性化调整设置，其中调整TSS所用到的数据都是前面程序累积压栈形成的参数。

```
    /* 只有内核代码中明确表示将该进程设置为就绪状态才能被唤醒 
     * 除此之外，没有任何方法能将其唤醒
    */
	p->state = TASK_UNINTERRUPTIBLE;   
    /* 开始子进程的个性化设置 */                 
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		                                /* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
    /* 开始设置子进程的TSS
    */
	p->tss.back_link = 0;
    /* esp0是内核栈指针 */
	p->tss.esp0 = PAGE_SIZE + (long) p;
    /* 堆栈0x10(10000)，0特权，GDT，数据段 */
	p->tss.ss0 = 0x10;
    /* 参数的EIP，int 0x80入栈的，指向if(__res >=0)*/
	p->tss.eip = eip;
	p->tss.eflags = eflags;
    /*  重要，决定了main()中if(!fork())的分支走向 */
	p->tss.eax = 0;
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
    /* 挂接子进程的LDT */
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000;
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
```

这两行代码为第二次执行fork()中的if (__res >= 0) 埋下伏笔。这个伏笔比较隐讳，不太容易看出来，请读者一定要记住这件事！

## 设置进程1的分页管理

Intel 80x86体系结构分页机制是基于保护模式的，先打开pe，才能打开pg，不存在没有pe的pg。保护模式是基于段的，换句话说，设置进程1的分页管理，就要先设置进程1的分段。

一般来讲，每个进程都要加载属于自己的代码、数据。这些代码、数据的寻址都是用段加偏移的形式，也就是逻辑地址形式表示的。CPU硬件自动将逻辑地址计算为CPU可寻址的线性地址，再根据操作系统对页目录表、页表的设置，自动将线性地址转换为分页的物理地址。操作系统正是沿着这个技术路线，先在进程1的64 MB线性地址空间中设置代码段、数据段，然后设置页表、页目录。

![processOne_02](../images/05_processOne/processOne_02.jpg#pig_center)

1. 在进程1的线性地址空间中设置代码段、数据段调用copy_mem()函数，先设置进程1的代码段、数据段的段基址、段限长，提取当前进程（进程0）的代码段、数据段以及段限长的信息，并设置进程1的代码段和数据段的基地址。这个基地址就是它的进程号nr*64 MB。设置新进程LDT中段描述符中的基地址，如图3-8中的第一步所示。

```

/* 取段选择符 segment 的段长值
*/
#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})
```

**lsll**: 数字逻辑左移指令，功能为将后面的操作数向左移动前面操作数指定的位数。（在该代码中，则是把 segment 左移3位）

**incl**: （Add one Addend to Long integer）是将addr所指内存或寄存器的数值加一。在这里就是把 edx累加1。

基于输入的段(segment)计算其在GDT表中的长度limit，并将之返回。

```
/* 设置新任务的代码和数据段基址、限长并复制页表。 
 * nr 为新任务号；p 是新任务数据结构的指针
*/
int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;

    /* 0x0f = 00001111: 获取代码段，LDT，3特权级
	code_limit=get_limit(0x0f);        
    /* 0x17 = 00010111: 数据段，LDT，3特权级
	data_limit=get_limit(0x17);       
```


```
// 取局部描述符表中 ldt 所指段描述符中的基地址
#define get_base(ldt) _get_base( ((char *)&(ldt)) )
#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )

    /* 获取父进程(现为进程0)的代码段、数据段基址
     *
    */
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
    /* 新基址=任务号*64Mb */
	new_data_base = new_code_base = nr * 0x4000000;   
	p->start_code = new_code_base;
    /* 设置子进程代码段基址 */
	set_base(p->ldt[1],new_code_base);  
    /* 设置子进程数据段基址 */  
	set_base(p->ldt[2],new_data_base);    
```

2. 为进程1创建第一个页表并设置对应的页目录项在Linux 0.11中，每个进程所属的程序代码执行时，都要根据其线性地址来进行寻址，并最终映射到物理内存上。通过图3-9我们可以看出，线性地址有32位，CPU将这个线性地址解析成“页目录项”、“页表项”和“页内偏移”；页目录项存在于页目录表中，用以管理页表；页表项存在于页表中，用以管理页面，最终在物理内存上找到指定的地址。Linux 0.11中仅有一个页目录表，通过线性地址中提供的“页目录项”数据就可以找到页目录表中对应的页目录项；通过这个页目录项就可以找到对应的页表；之后，通过线性地址中提供的“页表项”数据，就可以在该页表中找到对应的页表项；通过此页表项可以进一步找到对应的物理页面；最后，通过线性地址中提供的“页内偏移”落实到实际的物理地址值。


调用copy_page_tables()函数，设置页目录表和复制页表，如图中第二步和第三步所示，注意其中页目录项的位置。

进入copy_page_tables()函数后，先为新的页表申请一个空闲页面，并把进程0中第一个页表里面前160个页表项复制到这个页面中（1个页表项控制一个页面4 KB内存空间，160个页表项可以控制640 KB内存空间）。进程0和进程1的页表暂时都指向了相同的页面，意味着进程1也可以操作进程0的页面。之后对进程1的页目录表进行设置。最后，用重置CR3的方法刷新页变换高速缓存。进程1的页表和页目录表设置完毕。

```
/**
 * @brief 复制进程的页目录页表
 * 
 * @param from 源地址的内存偏移
 * @param to 目的地址的内存偏移
 * @param size 需要复制的内存大小
 * @return int 
 */
 
    int copy_page_tables(unsigned long from,unsigned long to,long size)   
    {
        // 进程1创建时 from = 0, to = 64M，  size = 640k或160个页面
        unsigned long * from_page_table;
        unsigned long * to_page_table;
        unsigned long this_page;
        unsigned long * from_dir, * to_dir;
        unsigned long nr;

        /* 源地址和目的地址都需要是 4Mb 的倍数。否则出错，死机 */
        if ((from&0x3fffff) || (to&0x3fffff)) 
            panic("copy_page_tables called with wrong alignment");
```
0x3fffff是4 MB，是一个页表的管辖范围，二进制是22个1，||的两边必须同为0，所以，from和to后22位必须都为0，即4 MB的整数倍，意思是一个页表对应4MB连续的线性地址空间必须是从0x000000开始的4 MB的整数倍的线性地址，不能是任意地址开始的4 MB，才符合分页的要求

```
        /* 取得源地址和目的地址的目录项(from_dir 和 to_dir)	 分别是0和64 */
        from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
        to_dir = (unsigned long *) ((to>>20) & 0xffc);
```

一个页目录项的管理范围是4 MB，一项是4字节，项的地址就是项数×4，也就是项管理的线性地址起始地址的M数，比如：0项的地址是0，管理范围是0～4MB，1项的地址是4，管理范围是4～8 MB，2项的地址是8，管理范围是8～12MB……>>20就是地址的MB数，&0xffc就是&111111111100b，就是4 MB以下部分清零的地址的MB数，也就是页目录项的地址


```
/* 刷新页变换高速缓冲。 
 * 为了提高地址转换的效率，CPU 将最近使用的页表数据存放在芯片中高速缓冲中。在修改过页表 
 * 信息之后，就需要刷新该缓冲区。这里使用重新加载页目录基址寄存器 cr3 的方法来进行刷新。
*/
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))
```

这是一段内联汇编的代码，在执行时，它会将EAX寄存器中的值加载到控制寄存器CR3中，强制处理器重新加载页目录表以使其无效。在其他修改映射关系的操作后需要调用这一指令来保证之后已经不存在的映像条目不产生意外作用。

```
        /* 计算要复制的内存块占用的页表数  4M（1个页目录项管理的页面大小 = 1024*4K）的数量 */
        size = ((unsigned) (size+0x3fffff)) >> 22;    
        for( ; size-->0 ; from_dir++,to_dir++) {
            /* 如果目的目录项指定的页表已经存在，死机 */
            if (1 & *to_dir)                 
                panic("copy_page_tables: already exist");
            /* 如果源目录项未被使用，不用复制，跳过 */
            if (!(1 & *from_dir))            
                continue;
            /* 只取高20位，即页表的地址 */
            from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
            /* 为目的页表取一页空闲内存. 关键！！是目的页表存储的地址 */
            if (!(to_page_table = (unsigned long *) get_free_page()))     
                return -1;	/* Out of memory, see freeing */
            /* 设置目的目录项信息。7 是标志信息，表示(Usr, R/W, Present) */
            *to_dir = ((unsigned long) to_page_table) | 7; 
            /* 如果是进程0复制给进程1，则复制160个页面；否则将1024个页面全部复制 */
            nr = (from==0)?0xA0:1024;
            for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {    
                this_page = *from_page_table;     // 复制！
                if (!(1 & this_page))
                    continue;
                /* 010， 代表用户，只读，存在 */
                this_page &= ~2;         
                *to_page_table = this_page;       // 复制！
                /* 1M以内的内核去不参与用户分页管理
                if (this_page > LOW_MEM) {    
                    *from_page_table = this_page;
                    this_page -= LOW_MEM;
                    this_page >>= 12;
                    mem_map[this_page]++;
                }
            }
        }
        invalidate();     // 刷新页变换高速缓冲
        return 0;
    }
```

进程1此时是一个空架子，还没有对应的程序，它的页表又是从进程0的页表复制过来的，它们管理的页面完全一致，也就是它暂时和进程0共享一套内存页面管理结构，如图3-10所示。等将来它有了自己的程序，再把关系解除，并重新组织自己的内存管理结构。

```

	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {       // 复制代码和数据段
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

    /* 设置子进程代码段，数据及创建复制子进程的第一个页表
     * 
    */
	if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
```

![processOne_03](../images/05_processOne/processOne_03.jpg#pig_center)

## 进程1共享进程0的文件

返回copy_process()函数中继续调整。设置task_struct中与文件相关的成员，包括打开了哪些文件p->filp[20]、进程0的“当前工作目录i节点结构”、“根目录i节点结构”以及“执行文件i节点结构”。虽然进程0中这些数值还都是空的，进程0只具备在主机中正常运算的能力，尚不具备与外设以文件形式进行交互的能力，但这种共享仍有意义，因为父子进程创建机制会把这种能力“遗传”给子进程。

```
	for (i=0; i<NR_OPEN;i++)
		if (f=p->filp[i])
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
    if (current->executable)
		current->executable->i_count++;
```

## 设置进程1在GDT中的表项

```
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
}

```

##  进程1处于就绪态

![processOne_03](../images/05_processOne/processOne_03.jpg#pig_center)


将进程1的状态设置为就绪态，使它可以参加进程调度，最后返回进程号1。请注意图3-11中间代表进程的进程条，其中，进程1已处在就绪态。执行代码如下：

```
    /* do this last, just in case */
    /* 复制完成之后置为可运行状态 */
    p->state = TASK_RUNNING;	             
	return last_pid;

```

![processOne_04](../images/05_processOne/processOne_04.jpg#pig_center)

至此，进程1的创建工作完成，进程1已经具备了进程0的全部能力，可以在主机中正常地运行。进程1创建完毕后，copy_process()函数执行完毕，返回sys_fork()中call _copy_process()的下一行执行，执行代码如下：

```
    /* copy_process返回至此，esp+=20 就是esp清20字节的栈，也就是清前面的gs,esi */
	addl $20,%esp
    /* edi, ebp, eax, 注意:内核栈还有数据，返回_system_call中的pushl %eax运行*/   
1:	ret
```

_sys_fork压栈的5个寄存器的值，就是清前面压的gs、esi、edi、ebp、eax，也就是copy_process( )的前5个参数。注意：eax对应的是copy_process( )的第一个参数nr，就是copy_process( )的返回值last_pid，即进程1的进程号。然后返回_system_call中的call _sys_call_table（,%eax,4）的下一行pushl %eax处继续执行。


先检查当前进程是否是进程0。注意：pushl %eax这行代码，将3.1.6节中返回的进程1的进程号压栈，之后到_ret_from_sys_call:处执行。

```
        /* sys_fork返回到此执行，eax是copy_process()的返回值last_pid
        */
        pushl %eax 
        /* 当前是进程 0 */
        movl _current,%eax
        /* 如果进程0不是就绪态，则进程调度 */
        cmpl $0,state(%eax)		    # state
        jne reschedule
        /* 如果进程0没有时间片，则进程调度 */
        cmpl $0,counter(%eax)		# counter
        je reschedule
    ret_from_sys_call:
        movl _current,%eax		# task[0] cannot have signals
        /* 如果当前是进程0，跳转到3执行 */
        cmpl _task,%eax
        je 3f
        cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
        jne 3f
        cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
        jne 3f
        movl signal(%eax),%ebx
        movl blocked(%eax),%ecx
        notl %ecx
        andl %ebx,%ecx
        bsfl %ecx,%ecx
        je 3f
        btrl %ecx,%ebx
        movl %ebx,signal(%eax)
        incl %ecx
        pushl %ecx
        call _do_signal
        popl %eax
        /* 将7个寄存器的值出栈给CPU
        */
    3:	popl %eax
        popl %ebx
        popl %ecx
        popl %edx
        pop %fs
        pop %es
        pop %ds
        /* 将int 0x80的中断入栈的寄存器SS,ESP,EFLAGS,CS，EIP逆序出栈*/
        iret                /* CS:EIP 指向fork()中int 0x80的下一行if(__res >=0)执行*/
```

由于当前进程是进程0，所以就跳转到标号3处，将压栈的各个寄存器数值还原。下图表示了init_task中清栈的这一过程。值得注意的是popl %eax这一行代码，这是将前面刚刚讲解过的pushl %eax压栈的进程1的进程号，恢复给CPU的eax，eax的值为“1”。

![processOne_05](../images/05_processOne/processOne_05.jpg#pig_center)

之后，iret中断返回，CPU硬件自动将int 0x80的中断时压的ss、esp、eflags、cs、eip的值按压栈的反序出栈给CPU对应寄存器，从0特权级的内核代码转换到3特权级的进程0代码执行，CS：EIP指向fork( )中int 0x80的下一行if（__res >=0）。

```
#define _syscall0(type,name) \
type name(void) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \     // int 0x80是所有系统调用函数的总入口
	: "=a" (__res) \                // 输出部分，将__res值赋给eax
	: "0" (__NR_##name)); \         // 输入部分，"0",同上寄存器，即eax
if (__res >= 0) \                   // int 0x80中断返回后，执行本行
	return (type) __res; \
errno = -__res; \
return -1; \
}

```

在讲述执行if（__res >= 0）前，先关注一下: " =a"（__res）。这行代码的意思是将__res的值赋给eax，所以if（__res >= 0）这一行代码，实际上就是判断此时eax的值是多少。我们刚刚介绍了，这时候eax里面的值是返回的进程1的进程号1，return（type）__res将“1”返回。回到3.1.1节中fork()函数的调用点if（!fork( )）处执行，!1为“假”，这样就不会执行到init()函数中，而是进程0继续执行，接下来就会执行到for（;;）pause( )。执行代码如下：

回到fork()函数的调用点if（!fork( )）处执行，!1为“假”，这样就不会执行到init()函数中，而是进程0继续执行，接下来就会执行到for（;;）pause( )。执行代码如下

```
static inline _syscall0(int,pause)

// pause()系统调用。转换当前任务的状态为可中断的等待状态，并重新调度。
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}


    if (!fork()) {
        init();
    }

    for(;;) pause();
}
```

# 内核第一次做进程调度

现在执行的是进程0的代码。从这里开始，进程0准备切换到进程1去执行。在Linux 0.11的进程调度机制中，通常有以下两种情况可以产生进程切换。
1. 允许进程运行的时间结束。
   
   进程在创建时，都被赋予了有限的时间片，以保证所有进程每次都只执行有限的时间。一旦进程的时间片被削减为0，就说明这个进程此次执行的时间用完了，立即切换到其他进程去执行，实现多进程轮流执行。
2. 进程的运行停止。
   当一个进程需要等待外设提供的数据，或等待其他程序的运行结果……或进程已经执行完毕时，在这些情况下，虽然还有剩余的时间片，但是进程不再具备进一步执行的“逻辑条件”了。如果还等着时钟中断产生后再切换到别的进程去执行，就是在浪费时间，应立即切换到其他进程去执行。
   
   这两种情况中任何一种情况出现，都会导致进程切换。
   
   进程0角色特殊。现在进程0切换到进程1既有第二种情况的意思，又有怠速进程的意思。我们会在3.3.1节中讲解怠速进程。
   
   进程0执行for（;;）pause( )，最终执行到schedule()函数切换到进程1，如图所示。


pause()函数的调用与fork()函数的调用一样，会执行到unistd.h中的syscall0，通过int 0x80中断，在system_call.s中的call _sys_call_table（,%eax,4）映射到sys_pause( )的系统调用函数去执行，具体步骤与调用fork()函数步骤类似。略有差别的是，fork()函数是用汇编写的，而sys_pause()函数是用C语言写的。


进入sys_pause()函数后，将进程0设置为可中断等待状态，如图中第一步所示，然后调用schedule()函数进行进程切换，执行代码如下：

![processOne_06](../images/05_processOne/processOne_06.jpg#pig_center)


在schedule()函数中，先分析当前有没有必要进行进程切换，如果有必要，再进行具体的切换操作。

![processOne_07](../images/05_processOne/processOne_07.jpg#pig_center)


```
#define LAST_TASK task[NR_TASKS-1]		// 任务数组中的最后一项任务
#define FIRST_TASK task[0]				// 任务 0 比较特殊，所以特意给它单独定义一个符号

extern long volatile jiffies;						// 从开机开始算起的滴答数（10ms/滴答）

#define SIGALRM		14      // Alarm             实时定时器报警

// 除了SIGKILL和SIGSTOP之外其他都是可阻塞的信号(…10111111111011111111b)
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */
// ========== 根据信号唤醒进程 ===========
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {                  // 如果任务的 alarm 时间已经过期(alarm<jiffies),在信号位图中置 SIGALRM 信号，然后清 alarm
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&        //则置任务为就绪状态
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}
```

首先依据task[64]这个结构，第一次遍历所有进程，只要地址指针不为空，就要针对它们的“报警定时值alarm”以及“信号位图signal”进行处理（我们会在后续章节详细讲解信号，这里先不深究）。在当前的情况下，这些处理还不会产生具体的效果，尤其是进程0此时并没有收到任何信号，它的状态是“可中断等待状态”，不可能转变为“就绪态”。

```
/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)   //get counter_max 值
				c = (*p)->counter, next = i;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)                //calculate counter
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

```



第二次遍历所有进程，比较进程的状态和时间片，找出处在就绪态且counter最大的进程。现在只有进程0和进程1，且进程0是可中断等待状态，不是就绪态，只有进程1处于就绪态，所以，执行switch_to（next），切换到进程1去执行，如图中的第一步所示。

```
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 * 切换任务
 */
#define switch_to(n) {\                 
struct {long a,b;} __tmp; \             // 为ljmp的CS、EIP准备数据结构
    /* 进程是当前进程，不必切换，退出 */
__asm__("cmpl %%ecx,_current\n\t" \    
	"je 1f\n\t" \
    /* EDX的低字节赋给_tmp.b，即把CS赋给.b*/
	"movw %%dx,%1\n\t" \
    /* 交换task[n]与task[current]*/
	"xchgl %%ecx,_current\n\t" \
    /* ljmp到__tmp， __tmp中有便宜、段选择符，但任务门忽略偏移 */
	"ljmp %0\n\t" \
    /* 比较上次是否用过协处理器 */
	"cmpl %%ecx,_last_task_used_math\n\t" \
	"jne 1f\n\t" \
	/* 清除CR0中的切换标志 */
    "clts\n" \
	"1:" \
    /* a对应EIP(忽略), b对应CS */
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
    /* EDX是TSS n的索引号，ECX即task[n] */
	"d" (_TSS(n)),"c" ((long) task[n])); \
}

```

程序将一直执行到"ljmp %0\n\t" 这一行。ljmp通过CPU的任务门机制并未实际使用任务门，将CPU的各个寄存器值保存在进程0的TSS中，将进程1的TSS数据以及LDT的代码段、数据段描述符数据恢复给CPU的各个寄存器，实现从0特权级的内核代码切换到3特权级的进程1代码执行，如图中的第二步所示。

接下来，轮到进程1执行，它将进一步构建环境，使进程能够以文件的形式与外设交互。需要提醒的是，pause()函数的调用是通过int 0x80中断从3特权级的进程0代码翻转到0特权级的内核代码执行的，在_system_call中的call _sys_call_table (,%eax,4) 中调用sys_pause()函数，并在sys_pause( )中的schedule( )中调用switch( )，在switch( )中ljmp进程1的代码执行。现在，switch( )中ljmp后面的代码还没有执行，call _sys_call_table (,%eax,4)后续的代码也还没有执行，int 0x80的中断没有返回。

# 轮转到进程1执行

在分析进程1如何开始执行之前，先回顾一下进程0创建进程1的过程。

调用copy_process函数时曾强调过，当时为进程1设置的tss.eip就是进程0调用fork( )创建进程1时int 0x80中断导致的CPU硬件自动压栈的ss、esp、eflags、cs、eip中的EIP值，这个值指向的是int 0x80的下一行代码的位置，即if（__res >= 0）。

![processOne_09](../images/05_processOne/processOne_09.png#pig_center)

CPU 规定，如果 ljmp 指令后面跟的是一个 tss 段，那么，会由硬件将当前各个寄存器的值保存在当前进程的 tss 中，并将新进程的 tss 信息加载到各个寄存器。

前面讲述的ljmp通过CPU的任务门机制自动将进程1的TSS的值恢复给CPU，自然也将其中的tss.eip恢复给CPU。现在CPU中的EIP指向的就是fork中的if（__res >=0）这一行，所以，进程1就要从这一行开始执行。

回顾前面3.1.3节中的介绍可知，此时的__res值，就是进程1的TSS中eax的值，这个值在3.1.3节中被写死为0，即p->tss.eax = 0，因此，当执行到return（type）__res这一行时，返回值是0，如图所示

![processOne_10](../images/05_processOne/processOne_10.jpg#pig_center)

返回后，执行到main()函数中if（!fork( )）这一行，! 0为“真”，调用init()函数！执行代码如下：

```
    if (!fork()) {
        init();
    }

    for(;;) pause();
}
```

```
static inline _syscall1(int,setup,void *,BIOS)     //0 for 0 variable, 1 for 1 variable

void init(void)
{
	int pid,i;

	setup((void *) &drive_info);
	(void) open("/dev/tty0",O_RDWR,0);                        // 返回的句柄号 0 -- stdin 标准输入设备
	(void) dup(0);                                            // 复制句柄，产生句柄 1 号 -- stdout 标准输出设备
	(void) dup(0);                                            // 复制句柄，产生句柄 2 号 -- stderr 标准出错输出设备
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);                               // 打印缓冲区块数和总字节数，每块 1024 字节
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {                                      // 进程1创建进程2！！！！
		close(0);
		if (open("/etc/rc",O_RDONLY,0))         // 以只读形式打开rc文件（配置文件），占据0号文件描述符
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);       // 加载shell程序. 此shell程序读取rc文件并启动update进程，用于同步缓冲区的数据到外设
		_exit(2);                                // 每隔一段时间，update程序会被唤醒
	}
	// shell加载普通文件 如/etc/rc 后会退出，所以下面需要重新启动一个shell
	// shell加载字符设备文件不会退出， 如/dev/tty0
	if (pid>0)                                                //parent process
		while (pid != wait(&i))                               //父进程等待子进程的结束。&i 是存放返回状态信息的位置。
			/* nothing */;
	while (1) {                                               //创建的子进程的执行已停止或终止,再创建一个子进程
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);     // 标准输入设备 stdin
			(void) dup(0);                         // 标准输出设备 stdout
			(void) dup(0);                         // 标准错误输出设备 stderr
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}

```
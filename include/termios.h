#ifndef _TERMIOS_H
#define _TERMIOS_H
//terminal io
// 该文件含有终端 I/O 接口定义， 包括终端，串口线， USB等字符终端设备


#define TTY_BUF_SIZE 1024       // tty 中的缓冲区长度

/* 0x54 is just a magic number to make these relatively uniqe ('T') */
// tty 设备的 ioctl 调用命令集。ioctl 将命令编码在低位字中。 
 // 下面名称 TC[*]的含义是 tty 控制命令。 
#define TCGETS		0x5401			// 取相应终端 termios 结构中的信息(参见 tcgetattr())
#define TCSETS		0x5402			// 设置相应终端 termios 结构中的信息(参见 tcsetattr()，TCSANOW)
#define TCSETSW		0x5403			// 在设置终端 termios 的信息之前，需要先等待输出队列中所有数据处理完
#define TCSETSF		0x5404			// 在设置 termios 的信息之前，需要先等待输出队列中所有数据处理完，并且刷新(清空)输入队列
#define TCGETA		0x5405			// 取相应终端 termio 结构中的信息(参见 tcgetattr())
#define TCSETA		0x5406			// 设置相应终端 termio 结构中的信息(参见 tcsetattr()，TCSANOW 选项)
#define TCSETAW		0x5407			// 在设置终端 termio 的信息之前，需要先等待输出队列中所有数据处理完
#define TCSETAF		0x5408			// 在设置 termio 的信息之前，需要先等待输出队列中所有数据处理完，并且刷新(清空)输入队列
#define TCSBRK		0x5409			// 等待输出队列处理完毕(空)，如果参数值是 0，则发送一个 break（参见 tcsendbreak()，tcdrain()）
#define TCXONC		0x540A			// 开始/停止控制。如果参数值是 0，则挂起输出；如果是 1，则重新开启挂起的输出；如果是 2，则挂起， 输入；如果是 3，则重新开启挂起的输入（参见 tcflow()）
#define TCFLSH		0x540B			//刷新已写输出但还没发送或已收但还没有读数据
// 下面名称 TIOC[*]的含义是 tty 输入输出控制命令
#define TIOCEXCL	0x540C			// 设置终端串行线路专用模式
#define TIOCNXCL	0x540D			// 复位终端串行线路专用模式
#define TIOCSCTTY	0x540E			// 设置 tty 为控制终端。(TIOCNOTTY - 禁止 tty 为控制终端)
#define TIOCGPGRP	0x540F			// 读取指定终端设备进程的组 id
#define TIOCSPGRP	0x5410			// 设置指定终端设备进程的组 id
#define TIOCOUTQ	0x5411			// 返回输出队列中还未送出的字符数
#define TIOCSTI		0x5412			// 模拟终端输入。该命令以一个指向字符的指针作为参数，并假装该字符是在终端上键入的
#define TIOCGWINSZ	0x5413			// 读取终端设备窗口大小信息
#define TIOCSWINSZ	0x5414			// 设置终端设备窗口大小信息
#define TIOCMGET	0x5415			// 返回 modem 状态控制引线的当前状态比特位标志集
#define TIOCMBIS	0x5416			// 设置单个 modem 状态控制引线的状态(true 或 false)(Individual control line Set)
#define TIOCMBIC	0x5417			// 复位单个 modem 状态控制引线的状态(Individual control line clear)
#define TIOCMSET	0x5418			// 设置 modem 状态引线的状态
#define TIOCGSOFTCAR	0x5419		// 读取软件载波检测标志(1 - 开启；0 - 关闭)
#define TIOCSSOFTCAR	0x541A		// 设置软件载波检测标志(1 - 开启；0 - 关闭
#define TIOCINQ		0x541B			// 返回输入队列中还未取走字符的数目

// 窗口大小(Window size)属性结构
// ioctls 中的 TIOCGWINSZ 和 TIOCSWINSZ 可用来读取或设置这些信息。
struct winsize {
	unsigned short ws_row;            // 窗口字符行数
	unsigned short ws_col;            // 窗口字符列数
	unsigned short ws_xpixel;         // 窗口宽度，象素值
	unsigned short ws_ypixel;         // 窗口高度，象素值
};

// AT&T 系统 V 的 termio 结构
#define NCC 8          // termio 结构中控制字符数组的长度
struct termio {
	unsigned short c_iflag;		/* input mode flags */
	unsigned short c_oflag;		/* output mode flags */
	unsigned short c_cflag;		/* control mode flags */
	unsigned short c_lflag;		/* local mode flags */
	unsigned char c_line;		/* line discipline */
	unsigned char c_cc[NCC];	/* control characters */
};

// POSIX 的 termios 结构
#define NCCS 17       // termios 结构中控制字符数组的长度
struct termios {
	unsigned long c_iflag;		/* input mode flags */
	unsigned long c_oflag;		/* output mode flags */
	unsigned long c_cflag;		/* control mode flags */
	unsigned long c_lflag;		/* local mode flags */
	unsigned char c_line;		/* line discipline */
	unsigned char c_cc[NCCS];	/* control characters */
};

/* c_cc characters */
// 以下是 c_cc 数组对应字符的索引值
#define VINTR 0			// c_cc[VINTR] = INTR (^C)，\003，中断字符
#define VQUIT 1			// c_cc[VQUIT] = QUIT (^\)，\034，退出字符
#define VERASE 2		// c_cc[VERASE] = ERASE (^H)，\010，擦除字符
#define VKILL 3			// c_cc[VKILL] = KILL (^U)，\025，终止字符
#define VEOF 4			// c_cc[VEOF] = EOF (^D)，\004，文件结束字符
#define VTIME 5			// c_cc[VTIME] = TIME (\0)，\0， 定时器值
#define VMIN 6			// c_cc[VMIN] = MIN (\1)，\1， 定时器值
#define VSWTC 7			// c_cc[VSWTC] = SWTC (\0)，\0， 交换字符
#define VSTART 8		// c_cc[VSTART] = START (^Q)，\021，开始字符
#define VSTOP 9			// c_cc[VSTOP] = STOP (^S)，\023，停止字符
#define VSUSP 10		// c_cc[VSUSP] = SUSP (^Z)，\032，挂起字符
#define VEOL 11			// c_cc[VEOL] = EOL (\0)，\0， 行结束字符
#define VREPRINT 12		// c_cc[VREPRINT] = REPRINT (^R)，\022，重显示字符
#define VDISCARD 13		// c_cc[VDISCARD] = DISCARD (^O)，\017，丢弃字符
#define VWERASE 14		// c_cc[VWERASE] = WERASE (^W)，\027，单词擦除字符
#define VLNEXT 15		// c_cc[VLNEXT] = LNEXT (^V)，\026，下一行字符
#define VEOL2 16		// c_cc[VEOL2] = EOL2 (\0)，\0， 行结束 2

/* c_iflag bits */
// termios 结构输入模式字段 c_iflag 各种标志的符号常数
#define IGNBRK	0000001		// 输入时忽略 BREAK 条件
#define BRKINT	0000002		// 在 BREAK 时产生 SIGINT 信号
#define IGNPAR	0000004		// 忽略奇偶校验出错的字符
#define PARMRK	0000010		// 标记奇偶校验错
#define INPCK	0000020		// 允许输入奇偶校验
#define ISTRIP	0000040		// 屏蔽字符第 8 位
#define INLCR	0000100		// 输入时将换行符 NL 映射成回车符 CR
#define IGNCR	0000200		// 忽略回车符 CR
#define ICRNL	0000400		// 在输入时将回车符 CR 映射成换行符 NL
#define IUCLC	0001000		// 在输入时将大写字符转换成小写字符
#define IXON	0002000		// 允许开始/停止（XON/XOFF）输出控制
#define IXANY	0004000		// 允许任何字符重启输出
#define IXOFF	0010000		// 允许开始/停止（XON/XOFF）输入控制
#define IMAXBEL	0020000		// 输入队列满时响铃

/* c_oflag bits */
// termios 结构中输出模式字段 c_oflag 各种标志的符号常数
#define OPOST	0000001		// 执行输出处理
#define OLCUC	0000002		// 在输出时将小写字符转换成大写字符
#define ONLCR	0000004		// 在输出时将换行符 NL 映射成回车-换行符 CR-NL
#define OCRNL	0000010		// 在输出时将回车符 CR 映射成换行符 NL
#define ONOCR	0000020		// 在 0 列不输出回车符 CR
#define ONLRET	0000040		// 换行符 NL 执行回车符的功能
#define OFILL	0000100		// 延迟时使用填充字符而不使用时间延迟
#define OFDEL	0000200		// 填充字符是 ASCII 码 DEL。如果未设置，则使用 ASCII NULL
#define NLDLY	0000400		// 选择换行延迟
#define   NL0	0000000		// 换行延迟类型 0
#define   NL1	0000400		// 换行延迟类型 1
#define CRDLY	0003000		// 选择回车延迟
#define   CR0	0000000		// 回车延迟类型 0
#define   CR1	0001000		// 回车延迟类型 1
#define   CR2	0002000		// 回车延迟类型 2
#define   CR3	0003000		// 回车延迟类型 3
#define TABDLY	0014000		// 选择水平制表延迟
#define   TAB0	0000000		// 水平制表延迟类型 0
#define   TAB1	0004000		// 水平制表延迟类型 1
#define   TAB2	0010000		// 水平制表延迟类型 2
#define   TAB3	0014000		// 水平制表延迟类型 3
#define   XTABS	0014000		// 将制表符 TAB 换成空格，该值表示空格数
#define BSDLY	0020000		// 选择退格延迟
#define   BS0	0000000		// 退格延迟类型 0
#define   BS1	0020000		// 退格延迟类型 1
#define VTDLY	0040000		// 纵向制表延迟
#define   VT0	0000000		// 纵向制表延迟类型 0
#define   VT1	0040000		// 纵向制表延迟类型 1
#define FFDLY	0040000		// 选择换页延迟
#define   FF0	0000000		// 换页延迟类型 0
#define   FF1	0040000		// 换页延迟类型 1

/* c_cflag bit meaning */
// termios 结构中控制模式标志字段 c_cflag 标志的符号常数（8 进制数）
#define CBAUD	0000017			// 传输速率位屏蔽码	
#define  B0	0000000		/* hang up 挂断路线*/
#define  B50	0000001			// 波特率 50
#define  B75	0000002			// 波特率 75
#define  B110	0000003			// 波特率 110
#define  B134	0000004			// 波特率 134
#define  B150	0000005			// 波特率 150
#define  B200	0000006			// 波特率 200
#define  B300	0000007			// 波特率 300
#define  B600	0000010			// 波特率 600
#define  B1200	0000011			// 波特率 1200
#define  B1800	0000012			// 波特率 1800
#define  B2400	0000013			// 波特率 2400
#define  B4800	0000014			// 波特率 4800
#define  B9600	0000015			// 波特率 9600
#define  B19200	0000016			// 波特率 19200
#define  B38400	0000017			// 波特率 38400
#define EXTA B19200             // 扩展波特率 A
#define EXTB B38400				// 扩展波特率 B
#define CSIZE	0000060			// 字符位宽度屏蔽码
#define   CS5	0000000			// 每字符 5 比特位
#define   CS6	0000020			// 每字符 6 比特位
#define   CS7	0000040			// 每字符 7 比特位
#define   CS8	0000060			// 每字符 8 比特位
#define CSTOPB	0000100			// 设置两个停止位，而不是 1 个
#define CREAD	0000200			// 允许接收
#define CPARENB	0000400			// 开启输出时产生奇偶位、输入时进行奇偶校验
#define CPARODD	0001000			// 输入/输入校验是奇校验
#define HUPCL	0002000			// 最后进程关闭后挂断
#define CLOCAL	0004000			// 忽略调制解调器(modem)控制线路
#define CIBAUD	03600000		/* input baud rate (not used) 输入波特率*/
#define CRTSCTS	020000000000		/* flow control 流控制*/

#define PARENB CPARENB			// 开启输出时产生奇偶位、输入时进行奇偶校验
#define PARODD CPARODD			// 输入/输入校验是奇校验

/* c_lflag bits */
// termios 结构中本地模式标志字段 c_lflag 的符号常数
#define ISIG	0000001			// 当收到字符 INTR、QUIT、SUSP 或 DSUSP，产生相应的信号
#define ICANON	0000002			// 开启规范模式（熟模式）
#define XCASE	0000004			// 若设置了 ICANON，则终端是大写字符的
#define ECHO	0000010			// 回显输入字符
#define ECHOE	0000020			// 若设置了 ICANON，则 ERASE/WERASE 将擦除前一字符/单词
#define ECHOK	0000040			// 若设置了 ICANON，则 KILL 字符将擦除当前行
#define ECHONL	0000100			// 如设置了 ICANON，则即使 ECHO 没有开启也回显 NL 字符
#define NOFLSH	0000200			// 当生成 SIGINT 和 SIGQUIT 信号时不刷新输入输出队列，当 
								// 生成 SIGSUSP 信号时，刷新输入队列
#define TOSTOP	0000400			// 发送 SIGTTOU 信号到后台进程的进程组，该后台进程试图写 
 								// 自己的控制终端
#define ECHOCTL	0001000			// 若设置了 ECHO，则除 TAB、NL、START 和 STOP 以外的 ASCII 
 								// 控制信号将被回显成象^X 式样，X 值是控制符+0x40
#define ECHOPRT	0002000			// 若设置了 ICANON 和 IECHO，则字符在擦除时将显示
#define ECHOKE	0004000			// 若设置了 ICANON，则 KILL 通过擦除行上的所有字符被回显
#define FLUSHO	0010000			// 输出被刷新。通过键入 DISCARD 字符，该标志被翻转
#define PENDIN	0040000			// 当下一个字符是读时，输入队列中的所有字符将被重显
#define IEXTEN	0100000			// 开启实现时定义的输入处理

/* modem lines */
/* modem 线路信号符号常数 */
#define TIOCM_LE	0x001
#define TIOCM_DTR	0x002
#define TIOCM_RTS	0x004
#define TIOCM_ST	0x008
#define TIOCM_SR	0x010
#define TIOCM_CTS	0x020
#define TIOCM_CAR	0x040
#define TIOCM_RNG	0x080
#define TIOCM_DSR	0x100
#define TIOCM_CD	TIOCM_CAR
#define TIOCM_RI	TIOCM_RNG

/* tcflow() and TCXONC use these */
#define	TCOOFF		0			// 挂起输出
#define	TCOON		1			// 重启被挂起的输出
#define	TCIOFF		2			// 系统传输一个 STOP 字符，使设备停止向系统传输数据
#define	TCION		3			// 系统传输一个 START 字符，使设备开始向系统传输数据

/* tcflush() and TCFLSH use these */
#define	TCIFLUSH	0			// 清接收到的数据但不读
#define	TCOFLUSH	1			// 清已写的数据但不传送
#define	TCIOFLUSH	2			// 清接收到的数据但不读。清已写的数据但不传送

/* tcsetattr uses these */
#define	TCSANOW		0			// 改变立即发生
#define	TCSADRAIN	1			// 改变在所有已写的输出被传输之后发生
#define	TCSAFLUSH	2			// 改变在所有已写的输出被传输之后并且在所有接收到但还没有读取的数据被丢弃之后发生

typedef int speed_t;			// 波特率数值类型

// 返回 termios_p 所指 termios 结构中的接收波特率
extern speed_t cfgetispeed(struct termios *termios_p);
// 发送波特率
extern speed_t cfgetospeed(struct termios *termios_p);
// 设置接收波特率
extern int cfsetispeed(struct termios *termios_p, speed_t speed);
// 设置发送波特率
extern int cfsetospeed(struct termios *termios_p, speed_t speed);
// 等待 fildes 所指对象已写输出数据被传送出去
extern int tcdrain(int fildes);
// 挂起/重启 fildes 所指对象数据的接收和发送
extern int tcflow(int fildes, int action);
// 丢弃 fildes 指定对象所有已写但还没传送以及所有已收到但还没有读取的数据
extern int tcflush(int fildes, int queue_selector);
// 获取与句柄 fildes 对应对象的参数，并将其保存在 termios_p 所指的地方
extern int tcgetattr(int fildes, struct termios *termios_p);
// 如果终端使用异步串行数据传输，则在一定时间内连续传输一系列 0 值比特位
extern int tcsendbreak(int fildes, int duration);
// 使用 termios 结构指针 termios_p 所指的数据，设置与终端相关的参数
extern int tcsetattr(int fildes, int optional_actions,
	struct termios *termios_p);

#endif

// 该文件中定义了一些访问段寄存器或与段寄存器有关的内存操作函数


//// 读取 fs 段（额外的段寄存器）中指定地址处的字节。 
 // 参数：addr - 指定的内存地址。 
 // %0 - (返回的字节_v)；%1 - (内存地址 addr)。 
 // 返回：返回内存 fs:[addr]处的字节。
extern inline unsigned char get_fs_byte(const char * addr)
{
	unsigned register char _v;

	__asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

//// 读取 fs 段中指定地址处的字。 
 // 参数：addr - 指定的内存地址。 
 // %0 - (返回的字_v)；%1 - (内存地址 addr)。 
 // 返回：返回内存 fs:[addr]处的字
extern inline unsigned short get_fs_word(const unsigned short *addr)
{
	unsigned short _v;

	__asm__ ("movw %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

extern inline unsigned long get_fs_long(const unsigned long *addr)
{
	unsigned long _v;

	__asm__ ("movl %%fs:%1,%0":"=r" (_v):"m" (*addr)); \
	return _v;
}

//功能：向用户空间中addr地址处写一个字节的内容
// 参数：val ：要写入的数据； addr ：用户空间中的逻辑地址
extern inline void put_fs_byte(char val,char *addr)
{
__asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}

//// 读取 fs 段中指定地址处的字。 
 // 参数：addr - 指定的内存地址。 
 // %0 - (返回的字_v)；%1 - (内存地址 addr)。 
 // 返回：返回内存 fs:[addr]处的字。
extern inline void put_fs_word(short val,short * addr)
{
__asm__ ("movw %0,%%fs:%1"::"r" (val),"m" (*addr));
}

//// 读取 fs 段中指定地址处的长字(4 字节)。 
 // 参数：addr - 指定的内存地址。 
 // %0 - (返回的长字_v)；%1 - (内存地址 addr)。 
 // 返回：返回内存 fs:[addr]处的长字。
extern inline void put_fs_long(unsigned long val,unsigned long * addr)
{
__asm__ ("movl %0,%%fs:%1"::"r" (val),"m" (*addr));
}

/*
 * Someone who knows GNU asm better than I should double check the followig.
 * It seems to work, but I don't know if I'm doing something subtly wrong.
 * --- TYT, 11/24/91
 * [ nothing wrong here, Linus ]
 */

//// 取 fs 段寄存器值(选择符)。 
 // 返回：fs 段寄存器值。
extern inline unsigned long get_fs() 
{
	unsigned short _v;
	__asm__("mov %%fs,%%ax":"=a" (_v):);
	return _v;
}

//// 取 ds 段寄存器值。 
 // 返回：ds 段寄存器值
extern inline unsigned long get_ds() 
{
	unsigned short _v;
	__asm__("mov %%ds,%%ax":"=a" (_v):);
	return _v;
}

//// 设置 fs 段寄存器。 
 // 参数：val - 段值（选择符）
extern inline void set_fs(unsigned long val)
{
	__asm__("mov %0,%%fs"::"a" ((unsigned short) val));
}


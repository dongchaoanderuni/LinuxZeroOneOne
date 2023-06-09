#ifndef _STRING_H_
#define _STRING_H_

// 该头文件中以内嵌函数的形式定义了所有字符串操作函数，为了提高执行速度使用了内嵌汇编程序

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

extern char * strerror(int errno);

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		(C) 1991 Linus Torvalds
 */
 
 //// 将一个字符串(src)拷贝到另一个字符串(dest)，直到遇到 NULL 字符后停止。 
 // 参数：dest - 目的字符串指针，src - 源字符串指针。 
 // %0 - esi(src)，%1 - edi(dest)。
extern inline char * strcpy(char * dest,const char *src)
{
__asm__("cld\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	::"S" (src),"D" (dest):"si","di","ax");
return dest;
}

//// 拷贝源字符串 count 个字节到目的字符串。 
 // 如果源串长度小于 count 个字节，就附加空字符(NULL)到目的字符串。 
 // 参数：dest - 目的字符串指针，src - 源字符串指针，count - 拷贝字节数。 
 // %0 - esi(src)，%1 - edi(dest)，%2 - ecx(count)
extern inline char * strncpy(char * dest,const char *src,int count)
{
__asm__("cld\n"
	"1:\tdecl %2\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"rep\n\t"
	"stosb\n"
	"2:"
	::"S" (src),"D" (dest),"c" (count):"si","di","ax","cx");
return dest;
}

//// 将源字符串拷贝到目的字符串的末尾处。 
 // 参数：dest - 目的字符串指针，src - 源字符串指针。 
 // %0 - esi(src)，%1 - edi(dest)，%2 - eax(0)，%3 - ecx(-1)
extern inline char * strcat(char * dest,const char * src)
{
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"decl %1\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	::"S" (src),"D" (dest),"a" (0),"c" (0xffffffff):"si","di","ax","cx");
return dest;
}

//// 将源字符串的 count 个字节复制到目的字符串的末尾处，最后添一空字符。 
 // 参数：dest - 目的字符串，src - 源字符串，count - 欲复制的字节数。 
 // %0 - esi(src)，%1 - edi(dest)，%2 - eax(0)，%3 - ecx(-1)，%4 - (count)
extern inline char * strncat(char * dest,const char * src,int count)
{
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"decl %1\n\t"
	"movl %4,%3\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %2,%2\n\t"
	"stosb"
	::"S" (src),"D" (dest),"a" (0),"c" (0xffffffff),"g" (count)
	:"si","di","ax","cx");
return dest;
}

//// 将一个字符串与另一个字符串进行比较。 
 // 参数：cs - 字符串 1，ct - 字符串 2。 
 // %0 - eax(__res)返回值，%1 - edi(cs)字符串 1 指针，%2 - esi(ct)字符串 2 指针。 
 // 返回：如果串 1 > 串 2，则返回 1；串 1 = 串 2，则返回 0；串 1 < 串 2，则返回-1
extern inline int strcmp(const char * cs,const char * ct)
{
register int __res __asm__("ax");
__asm__("cld\n"
	"1:\tlodsb\n\t"
	"scasb\n\t"
	"jne 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"jmp 3f\n"
	"2:\tmovl $1,%%eax\n\t"
	"jl 3f\n\t"
	"negl %%eax\n"
	"3:"
	:"=a" (__res):"D" (cs),"S" (ct):"si","di");
return __res;
}

//// 字符串 1 与字符串 2 的前 count 个字符进行比较。 
 // 参数：cs - 字符串 1，ct - 字符串 2，count - 比较的字符数。 
 // %0 - eax(__res)返回值，%1 - edi(cs)串 1 指针，%2 - esi(ct)串 2 指针，%3 - ecx(count)。 
 // 返回：如果串 1 > 串 2，则返回 1；串 1 = 串 2，则返回 0；串 1 < 串 2，则返回-1
extern inline int strncmp(const char * cs,const char * ct,int count)
{
register int __res __asm__("ax");
__asm__("cld\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"scasb\n\t"
	"jne 3f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %%eax,%%eax\n\t"
	"jmp 4f\n"
	"3:\tmovl $1,%%eax\n\t"
	"jl 4f\n\t"
	"negl %%eax\n"
	"4:"
	:"=a" (__res):"D" (cs),"S" (ct),"c" (count):"si","di","cx");
return __res;
}

// 在字符串中寻找第一个匹配的字符
// 返回：返回字符串中第一次出现匹配字符的指针。若没有找到匹配的字符，则返回空指针
extern inline char * strchr(const char * s,char c)
{
register char * __res __asm__("ax");
__asm__("cld\n\t"
	"movb %%al,%%ah\n"
	"1:\tlodsb\n\t"
	"cmpb %%ah,%%al\n\t"
	"je 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"movl $1,%1\n"
	"2:\tmovl %1,%0\n\t"
	"decl %0"
	:"=a" (__res):"S" (s),"0" (c):"si");
return __res;
}

//// 寻找字符串中指定字符最后一次出现的地方。（反向搜索字符串） 
 // 参数：s - 字符串，c - 欲寻找的字符。 
 // %0 - edx(__res)，%1 - edx(0)，%2 - esi(字符串指针 s)，%3 - eax(字符 c)。 
 // 返回：返回字符串中最后一次出现匹配字符的指针。若没有找到匹配的字符，则返回空指针
extern inline char * strrchr(const char * s,char c)
{
register char * __res __asm__("dx");
__asm__("cld\n\t"
	"movb %%al,%%ah\n"
	"1:\tlodsb\n\t"
	"cmpb %%ah,%%al\n\t"
	"jne 2f\n\t"
	"movl %%esi,%0\n\t"
	"decl %0\n"
	"2:\ttestb %%al,%%al\n\t"
	"jne 1b"
	:"=d" (__res):"0" (0),"S" (s),"a" (c):"ax","si");
return __res;
}

// cs中第一个不在ct中出现的字符下标
// strspn("ABCDEFG019874", "ABCD") => 4    前向匹配字数
extern inline int strspn(const char * cs, const char * ct)
{
register char * __res __asm__("si");
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 1b\n"
	"2:\tdecl %0"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res-cs;
}

// 检索字符串 str1 开头连续有几个字符都不含字符串 str2 中的字符
// strspn("ABCDEF4960910", "013") => 9     前向不匹配字数
extern inline int strcspn(const char * cs, const char * ct)
{
register char * __res __asm__("si");
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n"
	"2:\tdecl %0"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res-cs;
}

// 首个匹配字符
// strpbrk("abcde2fghi3jk4l", "34") = '3'
extern inline char * strpbrk(const char * cs,const char * ct)
{
register char * __res __asm__("si");
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n\t"
	"decl %0\n\t"
	"jmp 3f\n"
	"2:\txorl %0,%0\n"
	"3:"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res;
}

// 在字符串 1 中寻找首个匹配整个字符串 2 的字符串
// strstr("RUNOOB", "NO") => "NOOB"
extern inline char * strstr(const char * cs,const char * ct)
{
register char * __res __asm__("ax");
__asm__("cld\n\t" \
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"	/* NOTE! This also sets Z if searchstring='' */
	"movl %%ecx,%%edx\n"
	"1:\tmovl %4,%%edi\n\t"
	"movl %%esi,%%eax\n\t"
	"movl %%edx,%%ecx\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 2f\n\t"		/* also works for empty string, see above */
	"xchgl %%eax,%%esi\n\t"
	"incl %%esi\n\t"
	"cmpb $0,-1(%%eax)\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"2:"
	:"=a" (__res):"0" (0),"c" (0xffffffff),"S" (cs),"g" (ct)
	:"cx","dx","di","si");
return __res;
}

// 计算字符串长度
extern inline int strlen(const char * s)
{
register int __res __asm__("cx");
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %0\n\t"
	"decl %0"
	:"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):"di");
return __res;
}

extern char * ___strtok;

// 字符串s按照 ct 进行分割
// #include <string.h>
// main(){
//     char s[] = "ab-cd : ef;gh :i-jkl;mnop;qrs-tu: vwx-y;z";
//     char *delim = "-: ";
//     char *p;
//     printf("%s ", strtok(s, delim));
//     while((p = strtok(NULL, delim)))
//         printf("%s ", p);
//         printf("\n");
// }
// 执行结果：
// ab cd ef;gh i jkl;mnop;qrs tu vwx y;z     //－与:字符已经被\0 字符取代
extern inline char * strtok(char * s,const char * ct)
{
register char * __res __asm__("si");
__asm__("testl %1,%1\n\t"
	"jne 1f\n\t"
	"testl %0,%0\n\t"
	"je 8f\n\t"
	"movl %0,%1\n"
	"1:\txorl %0,%0\n\t"
	"movl $-1,%%ecx\n\t"
	"xorl %%eax,%%eax\n\t"
	"cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"je 7f\n\t"			/* empty delimeter-string */
	"movl %%ecx,%%edx\n"
	"2:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 7f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 2b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 7f\n\t"
	"movl %1,%0\n"
	"3:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 5f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 3b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 5f\n\t"
	"movb $0,(%1)\n\t"
	"incl %1\n\t"
	"jmp 6f\n"
	"5:\txorl %1,%1\n"
	"6:\tcmpb $0,(%0)\n\t"
	"jne 7f\n\t"
	"xorl %0,%0\n"
	"7:\ttestl %0,%0\n\t"
	"jne 8f\n\t"
	"movl %0,%1\n"
	"8:"
	:"=b" (__res),"=S" (___strtok)
	:"0" (___strtok),"1" (s),"g" (ct)
	:"ax","cx","dx","di");
return __res;
}

//// 内存块复制。从源地址 src 处开始复制 n 个字节到目的地址 dest 处
extern inline void * memcpy(void * dest,const void * src, int n)
{
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src),"D" (dest)
	:"cx","si","di");
return dest;
}

//// 内存块移动。同内存块复制，但考虑移动的方向
// char str[] = "memmove can be very useful......";
// memmove (str+20,str+15,11);
// 运行结果：
// memmove can be very very useful.
extern inline void * memmove(void * dest,const void * src, int n)
{
if (dest<src)
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src),"D" (dest)
	:"cx","si","di");
else
__asm__("std\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src+n-1),"D" (dest+n-1)
	:"cx","si","di");
return dest;
}

//// 比较 n 个字节的两块内存（两个字符串），即使遇上 NULL 字节也不停止比较
extern inline int memcmp(const void * cs,const void * ct,int count)
{
register int __res __asm__("ax");
__asm__("cld\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 1f\n\t"
	"movl $1,%%eax\n\t"
	"jl 1f\n\t"
	"negl %%eax\n"
	"1:"
	:"=a" (__res):"0" (0),"D" (cs),"S" (ct),"c" (count)
	:"si","di","cx");
return __res;
}

//// 在 n 字节大小的内存块(字符串)中寻找指定字符
extern inline void * memchr(const void * cs,char c,int count)
{
register void * __res __asm__("di");
if (!count)
	return NULL;
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 1f\n\t"
	"movl $1,%0\n"
	"1:\tdecl %0"
	:"=D" (__res):"a" (c),"D" (cs),"c" (count)
	:"cx");
return __res;
}

//// 用字符填写指定长度内存块。 
 // 用字符 c 填写 s 指向的内存区域，共填 count 字节。
extern inline void * memset(void * s,char c,int count)
{
__asm__("cld\n\t"
	"rep\n\t"
	"stosb"
	::"a" (c),"D" (s),"c" (count)
	:"cx","di");
return s;
}

#endif

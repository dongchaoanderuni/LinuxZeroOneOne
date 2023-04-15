@startuml

struct task_struct
{
    long state:0;
    long counter:15;
    long priority:15;
    long signal:0;
    struct sigaction sigaction[32]:{{},};
    long blocked:0;
    int exit_code:0;
    unsigned long start_code:0;
    unsigned long end_code:0;
    unsigned long end_data:0;
    unsigned long brk:0;
    unsigned long start_stack:0;
    long pid:0;
    long father:-1;
    long pgrp:0;
    long session:0;
    long leader:0;
    unsigned short uid:0;
    unsigned short euid:0;
    unsigned short suid:0;
    unsigned short gidL0;
    unsigned short egid:0;
    unsigned short sgid:0;
    long alarm:0;
    long utime:0;
    long stime:0;
    long cutime:0;
    long cstime:0;
    long start_time:0;
    unsigned short used_math:0;
    int tty:-1;
    unsigned short umask:0022;
    struct m_inode * pwd:NULL;	
    struct m_inode * root:NULL;
    struct m_inode * executable:NULL;
    unsigned long close_on_exec:0;
    struct file * filp[NR_OPEN]:{NULL,};
    struct desc_struct ldt[3]:{{0,0},{0x9f,0xc0fa00},{0x9f,0xc0f200}};
    struct tss_struct tss;
}

struct sigaction
{
	void (*sa_handler)(int):NULL;
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_restorer)(void);
}

struct file 
{ 
	unsigned short f_mode;      
	unsigned short f_flags;     
	unsigned short f_count;    
	struct m_inode * f_inode;   
	off_t f_pos;               
}

struct m_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
	struct task_struct * i_wait;   
	unsigned long i_atime;          
	unsigned long i_ctime;          
	unsigned short i_dev;          
	unsigned short i_num;         
	unsigned short i_count;        
	unsigned char i_lock;			
	unsigned char i_dirt;			
	unsigned char i_pipe;			
	unsigned char i_mount;        
	unsigned char i_seek;          
	unsigned char i_update;			
}

struct desc_struct 
{
	unsigned long a,b;
}

struct tss_struct 
{
	long	back_link:0;	
	long	esp0:PAGE_SIZE+(long)&init_task;									
	long	ss0:0x10;		
	long	esp1:0;									
	long	ss1:0;		
	long	esp2:0;								
	long	ss2:0;		
	long	cr3:(long)&pg_dir;									
	long	eip:0;									
	long	eflags:0;									
	long	eax:0;
    long    ecx:0;
    long    edx:0;
    long    ebx:0;						
	long	esp:0;									
	long	ebp:0;									
	long	esi:0;									
	long	edi:0;									
	long	es:0x17;		
	long	cs:0x17;		
	long	ss:0x17;		
	long	ds:0x17;		
	long	fs:0x17;		
	long	gs:0x17;		
	long	ldt:0x28(40);		
	long	trace_bitmap:(0x80000000);	
	struct  i387_struct i387:{};
}

task_struct - sigaction:包含 >
task_struct - m_inode:包含 >
task_struct - file:包含 >
task_struct - desc_struct:包含 >
task_struct - tss_struct:包含 >

@enduml
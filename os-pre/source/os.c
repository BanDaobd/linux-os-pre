
#include "os.h"
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#define MAP_ADDR 0x80000000

void sys_show(char * str, char color)
{
    uint32_t addr[]={0,SYS_CALL_SEG};
    __asm__ __volatile__("push %[color];push %[str];push %[id];lcall *(%[a]) "::
    [a]"r"(addr),[color]"m"(color),[str]"m"(str),[id]"r"(2));
}

//func 功能号，决定我们在屏幕上做什么（清屏之类的），这里只设置2用来显示任务切换
void do_syscall(int func,char * str, char color)
{
    static int row=0;
    if(func == 2)
    {
        //操作地址0xb8000可以向屏幕显示，每个显示的元素项用16位设置，16位中高八位颜色低八位ascii码
        //屏幕为80列25行的表格
        unsigned short * dest = (unsigned short *)0xb8000+80*row;

        while(*str)
            //显示格式，高8位为颜色
            *dest++=*str++ | (color<<8);
        //屏幕上显示区域为25行的，超过就清零并重新显示
        row = (row >= 25)?0 : row + 1;

        for(int i=0;i<0xffffff;i++)
        {
            ;
        }
    }
}

void task_0(void)
{
    char *str="taska:1234";
    uint8_t color=0;
    for(;;)
        sys_show(str,color++);
}

void task_1(void)
{
    char *str="taskb:5678";
    uint8_t color=0xff;
    for(;;)
        sys_show(str,color--);

}

//权限位的设置
#define PDE_P (1<<0)    //表项存在
#define PDE_W (1<<1)    //表项可写
#define PDE_U (1<<2)    //用户权限
#define PDE_PS (1<<7)   //表明表项做的是4M到4M的映射关系


//将虚拟地址0x80000000映射到这个数组，大小为4kb，便于观察地址映射前后的地址变化，需要4kb内存对齐，4*1024=4096
//使用二级页表映射，第一个页表索引前10位，第二个索引中间10位，物理页中偏移12位
uint8_t map_phy_buffer[4096] __attribute__((aligned(4096)))=
{
    0x36,
};

//页表
uint32_t pag_dir[1024] __attribute__((aligned(4096)))=
{
    //做恒等映射
    [0]=(0) | PDE_P | PDE_W | PDE_U | PDE_PS,
    //不支持在这里直接映射（不知道为何）0x800000000，在后面使用函数映射
};

//二级页表
static uint32_t pag_dir_2[1024] __attribute__((aligned(4096)))=
{
    0,
};

uint32_t task0_dpl0_stack[1024];
uint32_t task0_dpl3_stack[1024];
uint32_t task1_dpl0_stack[1024];
uint32_t task1_dpl3_stack[1024];

//进程LDT表，存储应用的代码段和数据段等信息，段寄存器改为从这里获取信息而不是GDT表
struct
{
    //
    uint16_t segment_limit, base_s, base_attr, base_limit;
}task0_ldt_table[2]__attribute__((aligned(8)))=
{
    [TASK_CODE_SEG / 8]={0xffff, 0x0000, 0xfa00,0x00cf},
    [TASK_DATA_SEG / 8]={0xffff, 0x0000, 0xf300,0x00cf},
};

struct
{
    uint16_t segment_limit, base_s, base_attr, base_limit;
}task1_ldt_table[2]__attribute__((aligned(8)))=
{
    [TASK_CODE_SEG / 8]={0xffff, 0x0000, 0xfa00,0x00cf},
    [TASK_DATA_SEG / 8]={0xffff, 0x0000, 0xf300,0x00cf},
};

//TSS表实现进程的上下文切换
//（上下文切换指取出将要运行的程序之前保存的状态并保存现在正在运行但将要切出的程序的状态）
//这些状态包括寄存器
uint32_t task0_tss[]=
{
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task0_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pag_dir,  (uint32_t)task_0/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task0_dpl3_stack + 4*1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK0_LDT_SEG, 0x0,
};

uint32_t task1_tss[]=
{
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task1_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pag_dir, (uint32_t)task_1/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, 
      (uint32_t)task1_dpl3_stack + 4*1024/* 函数局部变量的栈空间 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK1_LDT_SEG, 0x0,
};

//全局IDT表
struct
{
    uint16_t offset_l, selector, attr, offset_h;
}idt_table[256]__attribute__((aligned(8)));

//GDT表项，8字节对齐，第一个字节应为0，段限制设置为0x07fff（见汇编代码）
struct 
{
    //段限制、代码段起始位置，属性、代码段限制
    uint16_t segment_limit, base_s, base_attr, base_limit;
}gdt_table[256]__attribute__((aligned(8)))=
{
    //数组gdt[1]和[2]设为索引，其他数据自动填0
    [KERNEL_CODE_SEG / 8]={0xffff, 0x0000, 0x9a00, 0x00cf},//段寄存器CS访问, 第一个是segmentlimit，4GB，第二个是baseaddress，从0开始,第三个DPL权限
    [KERNEL_DATA_SEG / 8]={0xFFFF, 0x0000, 0x9200, 0x00cf},//段寄存器BS访问, 

    //内核代码通过压栈出栈跳转到应用代码
    [APP_CODE_SEG / 8]={0xffff, 0x0000, 0xfa00, 0x00cf},
    [APP_DATA_SEG / 8]={0xffff, 0x0000, 0xf300, 0x00cf},

    [TASK0_LDT_SEG / 8]={sizeof(task0_ldt_table)-1, 0x0, 0xe200, 0x00cf},
    [TASK1_LDT_SEG / 8]={sizeof(task1_ldt_table)-1, 0x0, 0xe200, 0x00cf},

    //这个表项是如何与task函数对应的呢，在汇编文件中
    [TASK0_TSS_SEL / 8]={0x0068, 0, 0xE900, 0x0},
    [TASK1_TSS_SEL / 8]={0x0068, 0, 0xE900, 0X0},

    //系统调用门
    [SYS_CALL_SEG / 8]={0x0000, KERNEL_CODE_SEG, 0xec03, 0},

    
};


//8253定时器定时中断配置，端口引脚绑定，具体查看相应手册
void outb(uint8_t data, uint16_t port)
{
    //下面的语句是内嵌汇编表达式，asm是一个宏定义表示声明一个内联汇编表达式，volatile可选，表明不允许GCC对其做优化
    __asm__ __volatile__("outb %[v], %[p]"::[p]"d"(port), [v]"a"(data));
}

void task_sched(void)
{
    static int task_tss=TASK0_TSS_SEL;

    task_tss=(task_tss==TASK0_TSS_SEL)?TASK1_TSS_SEL:TASK0_TSS_SEL;

    //需要跳转到那个选择子
    uint32_t addr[]={0,task_tss};
    //内联汇编，跳转
    __asm__ __volatile__("ljmpl *(%[a]) "::[a]"r"(addr));
    //由于此时是特权级0，GDT表的内核代码段可访问

}

void syscall_handler(void);//系统调用函数,用汇编写, 这里只是给GDT表项一个可引用的函数位置，否则.S文件中的函数名需要在.c中声明
void timer_int(void);//这个函数用汇编编写，这里只是给IDT表项一个可引用的函数位置，否则.S文件中的函数名需要在.c中声明
void os_init(void) {
    //IDT表配置，8259芯片配置
    //主片中断起始0x20，写到0x21端口
    //主片0xA0，到0xA1
    outb(0x11,0x20);//主片初始化，向20端口写入规定值
    outb(0x11,0xA0);//从片初始化，向A0写入规定值
    outb(0x20,0x21);//CPU内部中断向量占用向量号0~21，从通道IRQ0传来的中断信号进入IDT表项IDT[20]后进行中断查找
    outb(0x28,0xA1);//有8个引脚，表项配置到0x28
    outb(1<<2,0x21);//主片第二个管脚连了从片
    outb(2,0xa1);//从片第二个管脚连到主片上
    outb(0x1,0x21);//配置主片工作模式，见手册
    outb(0x1,0xA1);//配置从片工作模式，见手册
    outb(0xfe,0x21);//管脚IRQ0设置为0开中断，其他为1屏蔽中断
    outb(0xff,0xA1);//屏蔽从片中断

    //8253每隔100ms产生一次中断
    int tmo=(1193180 / 100);
    outb(0x36,0x43);
    outb((uint8_t)tmo,0x40);
    outb(tmo>>8,0x40);

    //芯片产生中断到CPU，信号到IDT表项，IDT表项选择子指向GDT表，通过GDT表在虚拟地址中寻找中断函数位置
    idt_table[0x20].offset_l=(uint32_t)timer_int &0xFFFF;
    idt_table[0x20].offset_h=(uint32_t)timer_int >>16;
    idt_table[0x20].selector=KERNEL_CODE_SEG;
    idt_table[0x20].attr=0X8E00;

    //设置GDT表中task选择子，还要将选择子传送给任务寄存器TR告诉操作系统执行的是哪个任务
    gdt_table[TASK0_TSS_SEL / 8].base_s=(uint16_t)(uint32_t)task0_tss;
    gdt_table[TASK1_TSS_SEL / 8].base_s=(uint16_t)(uint32_t)task1_tss;
    gdt_table[SYS_CALL_SEG / 8].segment_limit=(uint16_t)(uint32_t)syscall_handler;

    gdt_table[TASK0_LDT_SEG / 8].base_s=(uint16_t)(uint32_t)task0_ldt_table;
    gdt_table[TASK1_LDT_SEG / 8].base_s=(uint16_t)(uint32_t)task1_ldt_table;


    //取最高10位，指向二级页表
    pag_dir[MAP_ADDR>>22]=(uint32_t)pag_dir_2 | PDE_P | PDE_W | PDE_U;
    //取中间10位，0x3ff从最低位数10个1，指向物理地址
    pag_dir_2[MAP_ADDR>>12 &0x3ff]=(uint32_t)map_phy_buffer | PDE_P | PDE_W | PDE_U;

}


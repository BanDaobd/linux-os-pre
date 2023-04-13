/**
 * 功能：16位与32位的启动混合代码
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
	#include "os.h"

	// 声明本地以下符号是全局的，在其它源文件中可以访问
	.global _start, timer_int, syscall_handler, do_syscall

	.extern os_init, color, str, func

	// 指定以下的代码生成16位的机器指令，这样才能在启动时的实模式下运行
  	.code16

	// 以下是代码区
 	.text
_start: 
	//为cs赋0，不加也行，qmenu会自动为cs赋0
	//平坦模式，段寄存器置零
	mov $0, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %gs
	mov %ax, %fs
	mov $_start, %esp			//将0x7c00给到esp，为后续的C代码分配栈空间，x86从高地址向低地址压栈，在makefile中指定了代码起始位置为0x7c00
	
//给bios中的函数传递参数，参照对应的bios规范
read_self_all:
	mov $_start_32, %bx			//读取地址，0x7c00+0x0200=0x7e00
	mov $0x2, %cx			    //起始扇区号，从第二个扇区开始读
	mov $0x240, %ax  			//0x40给到ax，指定了扇区数量，2给到al表示读磁盘
	mov $0x80, %dx			    //告诉读哪块磁盘，0x80指第一块磁盘
	int $0x13 					//int13中断调用bios函数
	jc read_self_all  			//判断cf标志位是否为1（读取失败则跳回read_self_all到标志处）

	//进入保护模式
	cli							//关中断, 防止模式切换时产生中断
	lgdt gdt_desc				//使得GDTR寄存器识别到GDT表
	lidt idt_desc				//使得IDTR寄存器识别到IDT表

	mov $1, %eax
	lmsw %ax

	jmp $KERNEL_CODE_SEG, $_start_32//从实模式跳转到保护模式需要跳转一次以清空流水线,选择子CS设置为8，CS指向GDT内核表项，内核表项设置内核代码区参数


	.org 0x1fe					//org是位置指示符，下面的一句代码会放在磁盘位置0x1fe==510，空两格字节放0x55和0xAA
	
	.byte 0x55, 0xAA			//第一个放在510~511,第二个放在511~512  


	.code32
	.text
_start_32:
	mov $KERNEL_DATA_SEG, %ax	//之前实模式的平坦模型下将段寄存器设为0，现在段寄存器作为gdt表的索引需要修改
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %gs
	mov %ax, %fs
	mov $_start, %esp 

	call os_init

	//CR3寄存器存页表地址页表
	mov $pag_dir, %eax
	mov %eax, %cr3

	//配置4M映射需要CR4寄存器，第四位PSE控制是否需要4M到4M映射
	mov %cr4, %eax
	orl $(1<<4), %eax
	mov %eax, %cr4

	//CR0寄存器最高位控制页表是否打开
	mov %cr0, %eax
	orl $(1<<31), %eax
	mov %eax, %cr0
	
	//传递GDT表中的选择子给tr寄存器告知要运行哪一个任务
	//先运行task0的tss
	mov $TASK0_TSS_SEL, %ax
	ltr %ax

	mov $TASK0_LDT_SEG , %ax
	lldt %ax					//LDTR寄存器

	//利用特权级发生变换时的栈行为，iret自动弹出到寄存器
	push $TASK_DATA_SEG			//ESP程序选择子
	push $task0_dpl3_stack+1024*4//为task0配置栈空间,栈从高地址向低地址压栈，栈底为数组最后一个元素
	push $0x202					//EFLAG的IF标志位IF控制开关中断
	push $TASK_CODE_SEG			//CS代码段
	push $task_0_entry			//EIP相当于PC，弹出后PC跳转到任务处
	iret						//中断返回

timer_int:						//中断函数,处理完中断后需要向8259发送值,发生中断时都会跑到这个中断函数中执行代码，并且特权级为0，此时可以使用jmp实现task0和task1的上下文切换
	push %ds
	pusha						//保护现场，消除中断时寄存器值改变产生的影响
	mov $0x20, %al
	outb %al, $0x20				//outb只支持al寄存器

	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds

	call task_sched				//任务切换函数

	popa
	pop %ds
	iret

task_0_entry:
	mov %ss, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %gs
	mov %ax, %fs
	jmp task_0

syscall_handler:
	push %ds
	pusha

	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds

	mov %esp, %ebp
	push 13*4(%ebp)
	push 12*4(%ebp)
	push 11*4(%ebp)

	push 

	call do_syscall
	//多压入了3个值要调整栈指针位置
	add $(3*4), %esp

	popa
	pop %ds
	retf $(3*4)

gdt_desc:
	.word (256*8) - 1			//gdt表的界限，从0开始所以要减一,256*8十六进制值为0x7ff
	.long gdt_table				//gdt表的起始地址

idt_desc:
	.word (256*8) - 1			//gdt表的界限，从0开始所以要减一,值为0x7ff
	.long idt_table				//gdt表的起始地址
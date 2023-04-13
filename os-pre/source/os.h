/**
 * 功能：公共头文件
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
#ifndef OS_H
#define OS_H

#define KERNEL_CODE_SEG    (1 * 8)    //内核代码区索引
#define KERNEL_DATA_SEG    (2 * 8)   //内核数据区索引

#define APP_CODE_SEG       ((3*8) | 3)
#define APP_DATA_SEG       ((4*8) | 3) 

#define TASK0_TSS_SEL      (5 * 8)
#define TASK1_TSS_SEL      (6 * 8)

#define SYS_CALL_SEG       (7 * 8)

#define TASK0_LDT_SEG      (8 * 8)
#define TASK1_LDT_SEG      (9 * 8)

//应用LDT表的选择子，LDT表可以从0开始,CS\DS寄存器会判断从GDT表还是LDT表中查找对应的代码段和数据段，第2位为1从LDT,0和1位为1切换到特权级3
#define TASK_CODE_SEG      ((0*0) | 0x4 | 3)
#define TASK_DATA_SEG      ((1*8) | 0x4 | 3)

#endif // OS_H

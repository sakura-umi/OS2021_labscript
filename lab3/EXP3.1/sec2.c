// 头文件
#include <linux/string.h>

// 必备头文件
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
// task_struct引用
#include <linux/sched.h>
// for_each_process引用
#include <linux/sched/signal.h>
// 定时器引用
#include <linux/timer.h>
// pid_task引用
#include <linux/pid.h>

//Color
#define CLOSE printk("\033[0m");
#define RED printk("\033[31m");
#define GREEN printk("\033[42;34m");
#define YELLOW printk("\033[33m");
#define BLUE printk("\033[46;30m");
//End Color

// 许可证
// 该模块的LICENSE
MODULE_LICENSE("GPL");
// 该模块的作者
MODULE_AUTHOR("Sakura");
// 该模块的说明
MODULE_DESCRIPTION("This is a simple example!/n");


// 命令行传递参数
// 该模块需要传递的参数
static int loop = -1;
static pid_t pid = -1;
module_param(loop, int, 0644);
module_param(pid, int, 0644);


// func = 2时的定时器对象部分
// 创建定时器对象
struct timer_list my_timer_list;
// 定时器函数
void timer_function(struct timer_list *timer)
{
	// printk();
	struct task_struct* p;
	int count = 0;
	for_each_process(p)
	{
		if(p->mm == NULL) // 内核进程由于只运行在内核态, 因此不存在虚拟地址空间, 故而task_struct对象中的mm为NULL, 根据这个筛选内核进程.
			count++;
	}
	printk("count of kernel thread is %d .\n", count);
	mod_timer(&my_timer_list, jiffies + 5 * HZ);// 修改定时器超时时间, 让计时器等待五秒后再次调用回调函数.
}
// END of func = 2


/* 初始化入口
 * 模块安装时执行
 * 这里的__init 同样是宏定义，主要的目的在于
 * 告诉内核，加载该模块之后，可以回收init.text的区间
 */
static int __init lab3_init(void)
{
	struct task_struct *p;
	p = &init_task;
	int count = 0;
    // 输出信息，类似于printf()
    // printk适用于内核模块
    printk(KERN_ALERT"module init!\n");
	switch (loop)
	{
	case 1:
	{
		char prio_char[4] = "";
		printk("PID\t\tSTAT\tCOMMEND\n");
		for_each_process(p)
		{
			//printk("pid is %d\n", p->pid);
			if(p->mm == NULL) //内核进程由于只运行在内核态, 因此不存在虚拟地址空间, 故而task_struct对象中的mm为NULL, 根据这个筛选内核进程.
			{
				//匹配和ps相符的状态码
				
				if(p->state == 1026)
					if(p->static_prio > 100)
						strncpy(prio_char, "I", 4);
					else
						strncpy(prio_char, "I<", 4);
				else
					if(p->static_prio > 120)
						strncpy(prio_char, "SN", 4);
					else if(p->static_prio < 120)
						strncpy(prio_char, "S", 4);
					else
						strncpy(prio_char, "S<", 4);
						
				printk("%d\t\t%.3s\t[%.16s]\t test is %d\n", p->pid, prio_char, p->comm, count);
				count++;
			}
		}
		printk("count of kernel thread is %d .\n", count);
		break;
	}
	case 2:
		timer_setup(&my_timer_list, timer_function, 0);
		my_timer_list.expires = jiffies + HZ; // 超时时间设置为1s, 1s后调用回调函数, 并根据回调函数内容(mod_timer)继续等待.
		add_timer(&my_timer_list); // 启动定时器.
		break;
	case 3:
	{
		// list_head 头指针
		struct list_head* head;
		// 寻找目标pid所指向的任务
		struct task_struct* target_task;
		struct task_struct* psibling;
		struct task_struct* pthread;
		target_task = pid_task(find_vpid(pid), PIDTYPE_PID);
		
		// Parent父进程信息
		printk("Parent INFO:\t");
		if(target_task->parent == NULL)
		{
			printk("    NO Parent!\n");
		}
		else
		{
			printk("    父进程 : \tPID = %d, STAT = %ld, COMMEND = %.16s\n", target_task->parent->pid, target_task->parent->state, target_task->parent->comm);
		}

		// Sibling兄弟进程信息
		int sibling_count = 0;
		printk("Siblings INFO:\t");
		list_for_each(head, &target_task->parent->children)
		{
			psibling = list_entry(head, struct task_struct, sibling);
			printk("    兄弟进程Num.%d :\tPID = %d, STAT = %ld, COMMEND = %.16s\n", ++sibling_count, psibling->pid, psibling->state, psibling->comm);
		}

		//Children子进程
		int children_count = 0;
		int pthread_count = 0;
		printk("Children INFO:\t");
		list_for_each(head, &target_task->children)
		{
			psibling = list_entry(head, struct task_struct, sibling);
			printk("    子进程Num.%d : \tPID = %d, STAT = %ld, COMMEND = %.16s\n", ++children_count, psibling->pid, psibling->state, psibling->comm);
			// 子进程Pthread线程
			pthread = psibling;
			do{
			printk("        子进程线程Num.%d  :  \tPID = %d, STAT = %ld, COMMEND = %.16s\n", ++pthread_count, pthread->pid, pthread->state, pthread->comm);
			}while_each_thread(psibling, pthread);
		}


		// Pthread线程
		pthread_count = 0;
		printk("Pthread INFO:\t");
		pthread = target_task;
		do{
			printk("    线程Num.%d  :  \tPID = %d, STAT = %ld, COMMEND = %.16s\n", ++pthread_count, pthread->pid, pthread->state, pthread->comm);
		}while_each_thread(target_task, pthread);

		/* 
		 * task_struct* p->parent
		 * list_head p->parent->children 
		 * list_head p->children
		 */
		break;
	}
	default:
		break;
	}
    return 0;
}

// 模块卸载时执行
static void __exit lab3_exit(void)
{
    printk(KERN_ALERT" module has exited!\n");
	del_timer_sync(&my_timer_list);
}

// 模块初始化宏，用于加载该模块
module_init(lab3_init);
// 模块卸载宏，用于卸载该模块
module_exit(lab3_exit);

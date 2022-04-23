#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/timekeeping.h>
#include <linux/sched/signal.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>

static int pid = 1;
module_param(pid, int, 0);
u64 start_time;
static struct task_struct *task;
static struct hrtimer  my_hrtimer;
static int exit = 0;

static unsigned long timer_interval_ns = 10e9; // 10-second timer


int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep){
    int ret = 0;
    if (pte_young(*ptep))
        ret = test_and_clear_bit(_PAGE_BIT_ACCESSED,
                     (unsigned long *) &ptep->pte);
    return ret;
}


int find_pte(struct task_struct *task){
    struct vm_area_struct *vma;
    unsigned long size;
    unsigned long address;
    pgd_t *pgd;
    p4d_t *p4d;
    pmd_t *pmd;
    pud_t *pud;
    pte_t *ptep, pte;

    for_each_process(task){
        //printk(KERN_INFO "process's pid = %d", task->pid);       
        if(task->pid == pid && task != NULL){
            printk("FIND PROCESS\n");
            vma = task->mm->mmap;
            address = vma->vm_start;

            int vma_counter = 0;
            unsigned long end = vma->vm_end; 
            int accessed = 0;
            int no_accessed = 0;
            int counter = 0;    //found in the memory
            int failed = 0;

            while(vma->vm_next != NULL){
                //int a = 0;      //accessed in each vma
                //int na = 0;     //no_accessed in each vma
                //int counter = 0;
                //int failed = 0;
                int i = 0;
                //iterate each vma
                for(address = vma->vm_start; address < vma->vm_end; address += PAGE_SIZE){
                    i++;            //memory region counter
                    pgd = pgd_offset(task->mm, address);                    // get pgd from mm and the page address
                    if (pgd_none(*pgd) || pgd_bad(*pgd)){           // check if pgd is bad or does not exist
                        failed++;
                    }
                    p4d = p4d_offset(pgd, address);                   // get p4d from from pgd and the page address
                    if (p4d_none(*p4d) || p4d_bad(*p4d)){          // check if p4d is bad or does not exist
                        failed++;
                    }
                    pud = pud_offset(p4d, address);                   // get pud from from p4d and the page address
                    if (pud_none(*pud) || pud_bad(*pud)){          // check if pud is bad or does not exist
                        failed++;
                    }
                    pmd = pmd_offset(pud, address);               // get pmd from from pud and the page address
                    if (pmd_none(*pmd) || pmd_bad(*pmd)){       // check if pmd is bad or does not exist
                        failed++;
                    } 
                    ptep = pte_offset_map(pmd, address);      // get pte from pmd and the page address
                    if (!ptep){
                        failed++;
                    }                                         // check if pte does not exist
                    pte = *ptep;
                    if(ptep_test_and_clear_young(vma, address, ptep) ==  1){
                        accessed++;
                        //a++;
                    }else{
                        no_accessed++;
                        //na++;
                    }            
                    if(pte_present(pte) == 1){
                        counter++;
                    }else{
                        failed++;
                    }           
                }
                vma_counter++;
                //printk("The %dth vma", vma_counter);
                //printk("%d pages found in memory, %d pages failed to found in memory, in total %d\n", counter, failed, i);
                //printk("%d pages get accessed, %d didn't", accessed, no_accessed);
                //printk("tag1");
                vma = vma->vm_next;
                //printk("tag2");   
            }
            int wss_size = accessed * 4;
            int rss_size = counter * 4;
            int swap_size = failed * 4;
            //printk("In total, %d pte get accessed, %d didn't", accessed, no_accessed);
            //printk("%d pages in the memory, %d pages in the storage, %d is being accessing", counter, failed, accessed);
            printk("PID = %d: RSS = %dKB, WSS = %dKB, SWAP = %dKB", pid, rss_size, wss_size, swap_size);
        }
        //printk("tag3");
    }
    return 0;
}

static enum hrtimer_restart no_restart_callback(struct hrtimer *timer) {
    exit++;
    ktime_t currtime , interval;
  	currtime  = ktime_get();
  	interval = ktime_set(0, timer_interval_ns); 
  	hrtimer_forward(timer, currtime , interval);
	find_pte(task);
    printk("%d", exit);
    
	return HRTIMER_RESTART;

}


static int __init mm_init(void){
    printk(KERN_INFO "initialize");
    ktime_t ktime;
	ktime = ktime_set( 0, timer_interval_ns );
	hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	my_hrtimer.function = &no_restart_callback;
	//start_time = jiffies;
	hrtimer_start(&my_hrtimer, ktime, HRTIMER_MODE_REL);   
    return 0;
}


void __exit mm_exit(void){
    int ret;
  	ret = hrtimer_cancel(&my_hrtimer);
  	if (ret) printk("The timer was still in use...\n");
  	printk("HR Timer module uninstalling\n");
}



MODULE_LICENSE("GPL");
module_init(mm_init);
module_exit(mm_exit);
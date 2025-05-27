#include <inc/lib.h>

#define SNAPSHOT_SIZE (5 * PGSIZE)

void* alloc_snapshot(envid_t child) {
    uintptr_t va = (uintptr_t) UTEMP; // Use UTEMP as the base address for alignment
    int i;

    for (i = 0; i < SNAPSHOT_SIZE; i += PGSIZE) {
        cprintf("alloc_snapshot: Allocating page at va = %p\n", (void*)(va + i));
        int r = sys_page_alloc(0, (void*)(va + i), PTE_P|PTE_U|PTE_W);
        if (r < 0)
            panic("alloc_snapshot: sys_page_alloc failed: %e", r);

        // Map the allocated page in the child environment
        r = sys_page_map(0, (void*)(va + i), child, (void*)(va + i), PTE_P|PTE_U);
        if (r < 0)
            panic("alloc_snapshot: sys_page_map failed: %e", r);
    }
    return (void*) va;
}



void
child_main() {
    int counter = 0;
    char c;
    
    while (1) {
        if(counter % 1000 == 0 )
            cprintf("\nChild state: counter = %d\n", counter);
      
        
       
        counter++;

        
        sys_yield();
    } 
}

void
umain(int argc, char **argv) {
    envid_t child = fork();
    if (child == 0) {
        child_main();
        return;
    }

    void* snapshot = alloc_snapshot(child);
    int i;
    for (i = 0; i < 3; ++i) sys_yield();

    cprintf("\nParent: Taking snapshot...\n");
    sys_env_snapshot(child, snapshot);

    cprintf("\nParent: Continuing child execution...\n");
    for (i = 0; i < 5; ++i) sys_yield();

    cprintf("\nParent: Restoring to previous snapshot...\n");
    sys_env_restore(child, snapshot);
    
    cprintf("\nParent: Child restored to previous state!\n");
    while (1) sys_yield();
}

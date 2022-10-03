#ifndef VM_FRAME_H
#define VM_FRAME_H


struct page
{
    struct list_elem page_elem; /* page_list element */
    struct thread* owner;
    

};


















#endif /* vm/page.h */
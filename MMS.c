//Author: idkwhodatis


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGES 256 //2^16/256=256
#define PAGE_SIZE 256
#define OFFSET_BITS 8
#define OFFSET_MASK 255

int page_table[PAGES];
int page_table_rev[128];
//physical memory 128 pages*256 bytes per page
signed char values[128*256];
signed char *ptr;
int search_TLB(int page_n);
int TLB_Add(int page_n,int frame_n);
int Handling_Page_Faults();
int updated=0;

//MMU – Address Translation
int Addr_Trans(){
    FILE *fp;
    char s[10];
    int page_n;
    int page_o;
    int virtual_addr;
    int frame_n;
    int physical_addr;
    int total_addr=0;
    int page_faults=0;
    int tlb_hits=0;

    //check if the file exists
    fp=fopen("addresses.txt","r");
    if(fp==NULL){
        return -1;
    }
    //loop until reach end of file
    while(!feof(fp)){
        total_addr++;
        fgets(s,10,fp);
        virtual_addr=atoi(s);
        page_n=virtual_addr>>OFFSET_BITS;
        page_o=virtual_addr&OFFSET_MASK;
        //check if page number is in TLB
        if((frame_n=search_TLB(page_n))==-1){
            //check if page number is in page table
            if((frame_n=page_table[page_n])==-1){
                page_faults++;
                frame_n=Handling_Page_Faults(page_n);
            }
            //in case the frame number is not assigned to an exist TLB entry
            if(!updated){
                TLB_Add(page_n,frame_n);
            }
        }else{
            tlb_hits++;
        }
        physical_addr=(frame_n<<OFFSET_BITS)|page_o;
        printf("Virtual address: %d Physical address = %d Value=%hhd\n",virtual_addr,physical_addr,*(values+physical_addr));
    }
    fclose(fp);
    printf("Total addresses = %d\n",total_addr);
    printf("Page_faults = %d\n",page_faults);
    printf("TLB Hits = %d\n",tlb_hits);
    return 0;
}

//MMU - TLB
typedef struct{
    int page_n;
    int frame_n;
}TLBentry;

//pointer to achieve circular array and FIFO
int tlb_curr=0;
TLBentry tlb[16];

int search_TLB(int page_n){
    for(int i=0;i<16;i++){
        if(tlb[i].page_n==page_n){
            //return the corresponding frame number for checking if it's reset to -1
            return tlb[i].frame_n;
        }
    }
    return -1;
}

int TLB_Add(int page_n,int frame_n){
    tlb[tlb_curr].page_n=page_n;
    tlb[tlb_curr].frame_n=frame_n;
    tlb_curr++;
    //circular array
    if(tlb_curr>=16){
        tlb_curr=0;
    }
    return 0;
}

int TLB_Update(int page_n,int frame_n){
    for(int i=0;i<16;i++){
        if(tlb[i].page_n==page_n){
            tlb[i].frame_n=frame_n;
            return 0;
        }
    }
    return -1;
}

//pointers to achieve circular array and FIFO
int physical_num=0;
int physical_curr=0;

//Handling Page Faults 3. 4. 5.
int Handling_Page_Faults(int page_n){
    int frame_n;
    int old;
    size_t size;
    // 2^15/256=128
    //create new page
    if(physical_num<128){
        frame_n=physical_num;
        physical_num++;
    //replace the oldest
    }else{
        frame_n=physical_curr;
        //reset index to 0 when all pages are used(circular array)
        if(++physical_curr>=128){
            physical_curr=0;
        }
        //reset old entry
        old=page_table_rev[frame_n];
        page_table[old]=-1;
        //check if old is in TLB and replace it
        if(search_TLB(old)!=-1){
            TLB_Update(old,frame_n);
        }
    }
    size=256;
    //values[frame_n]=ptr[page_n]
    memcpy(values+frame_n*size,ptr+page_n*size,size);
    page_table[page_n]=frame_n;
    page_table_rev[frame_n]=page_n;
    return frame_n;
}

int main(){
    //Handling Page Faults 1. 2.
    int fd;
    size_t size;

    //init all frame number to -1
    memset(page_table,-1,sizeof page_table);

    //open and load back up file to the memory
    fd=open("BACKING_STORE.bin",O_RDONLY);
    size=65536;
    ptr=mmap(NULL,size,PROT_READ,MAP_PRIVATE,fd,0);

    //init all TLB entry page number to -1
    for(int i=0;i<16;i++){
        tlb[i].page_n=-1;
    }

    Addr_Trans();

    munmap(ptr,size);
    close(fd);
    return 0;
}

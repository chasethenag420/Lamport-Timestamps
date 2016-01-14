#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <net/protocol.h>
#include <net/route.h>
#include <net/ip.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/fs.h>
#include <net/sock.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#define CSE536_MAJOR 234
#define PROTOCOL_NUM 234 
#define DATA_SIZE 256
#define DATA_LENGTH 236
#define HEADER_LEN 100
#define TIMEOUT 5000
static int debug_enable=0;
module_param(debug_enable,int,0);
MODULE_PARM_DESC(debug_enable,"Enable module debug mode.");
struct file_operations cse536_fops;
int handler(struct sk_buff *skbuffer);
void error(struct sk_buff *skb,__u32 info);
// struct to register module functions to ip layer
static const struct net_protocol new_protocol={
    .handler=handler,
    .err_handler=error,
    .no_policy=1,
    .netns_ok=1,
};
struct semaphore mutex_rcv, mutex;

uint32_t LocalClock=0;
uint32_t ackID;
typedef struct Message{

    uint32_t recordID;
    uint32_t finalClock;
    uint32_t origClock;
    __be32 source;
    __be32 destination;
    uint8_t msgData[DATA_LENGTH];
}Message;


//create a linked list to store incoming messages in buffer and declare variables for head,tail,next to traverse
struct buffer{
    struct buffer *next;
    //char packet[DATA_SIZE];
    Message *message;
} *bufferhead=0,*buffertail=0,*next=0;
uint32_t finalClock=0;
uint32_t local_origClock=0;
__be32 destination,source;
uint32_t update_LocalClock(uint32_t rcvd_origClock){


    LocalClock++;
    if(LocalClock < rcvd_origClock){
        LocalClock=rcvd_origClock+1;
    }else if(LocalClock == rcvd_origClock){
        LocalClock++;
    }
     finalClock=LocalClock;


    return finalClock;

}

static int cse536_open(struct inode *inode,struct file *file){
    printk("cse536_open: successful\n");
    return 0;
}
static int cse536_release(struct inode *inode,struct file *file){
    printk("cse536_release: successful\n");
    return 0;
}
static ssize_t cse536_read(struct file *file,char *buf,size_t count,loff_t *ptr){	
    ssize_t length=0 ;
    next=NULL;

    if(bufferhead !=0){	//If packet is there in buffer read it
        memset(buf, 0, 256);
        length = 1;
        memcpy(buf, &((bufferhead->message)->recordID), 4);
        memcpy(buf+4,&((bufferhead->message)->finalClock), 4);
        memcpy(buf+8,&((bufferhead->message)->origClock), 4);
        memcpy(buf+12,&((bufferhead->message)->source), 4);
        memcpy(buf+16,&((bufferhead->message)->destination), 4);
        memcpy(buf+20, &((bufferhead->message)->msgData), 236);
        printk("cse536 Read: Record ID %u Final Clock %u Orig Clock %u Message %s\n",(bufferhead->message)->recordID,(bufferhead->message)->finalClock,(bufferhead->message)->origClock,(bufferhead->message)->msgData);

        next=bufferhead->next;
        kfree(bufferhead);
        if(next==0){//If last node set head and tail to 0
            bufferhead=0;
            buffertail=0;
        }else
            bufferhead=next;
    }
    return length?DATA_SIZE:0;
}

void push(const char *buf,size_t count,uint32_t type,__be32 destination_ip ,Message* msgRcvd){
    Message *msg;
    struct sk_buff *skbuffer;
    struct iphdr *ipheader;
    struct rtable *routetable;
    uint32_t * origclockaddr=0;
    int sizeofipheader=sizeof(struct iphdr);
    skbuffer=alloc_skb(sizeofipheader + 4096,GFP_KERNEL);// create and setup an sk_buff
    skb_reserve(skbuffer,sizeofipheader + 1500);// reserve space for other layers to add headers
    msg=(Message*)kmalloc(sizeof(Message), GFP_KERNEL);
    memset(msg->msgData, 0, sizeof(msg->msgData));
    msg = (Message*)skb_put(skbuffer, sizeof(Message));				//make space for user data


    if(type==0){
        memset(msg->msgData, 0, sizeof(msg->msgData));
        memcpy(msg->msgData, buf, count);
        (msg)->recordID=type;
        (msg)->origClock=msgRcvd->origClock;
         (msg)->finalClock=LocalClock;
        (msg)->source=destination_ip;
        (msg)->destination=source;
    }
    if(type==1){


       LocalClock++;
       local_origClock++;
        if(copy_from_user( msg , buf, 256) ) {		//	Copy data to buffer
            printk("cse536 ERROR: User Fault while copying data to msg");

        }
        memcpy(&(msg->msgData),&(msg->recordID)+5,236);
        memcpy(&origclockaddr,&(msg->recordID)+1,8);
        put_user(LocalClock,origclockaddr);
        put_user(local_origClock,origclockaddr+1);
        (msg)->recordID=type;
        (msg)->origClock=local_origClock;
         (msg)->finalClock=LocalClock;
        (msg)->source=source;
        (msg)->destination=destination_ip;

    }



    printk("cse536 PUSH: Record ID %u Final Clock %u Orig Clock %u Message %s\n",(msg)->recordID,(msg)->finalClock,(msg)->origClock,msg->msgData);

    skb_push(skbuffer,sizeofipheader);// setup and add the ip header
    skb_reset_network_header(skbuffer);
    ipheader=ip_hdr(skbuffer);
    /* IP Header */
    ipheader->ihl = 5;
    ipheader->version = 4;
    ipheader->tos = 16; // Low delay
    ipheader->id = htons(54321);
    ipheader->ttl = 128; // hops
    ipheader->protocol = PROTOCOL_NUM; // new protocol
    /* Source IP address, can be spoofed */
    ipheader->saddr = source;
    ipheader->frag_off=0;
    ipheader->daddr = destination_ip;
    ipheader->tot_len=htons(skbuffer->len);
    routetable=ip_route_output(&init_net,destination_ip,source,0,0);	// get the route.
    skb_dst_set(skbuffer,&routetable->dst);
    ip_local_out(skbuffer);
}
static ssize_t cse536_write(struct file *file, const char *buf,size_t count,loff_t * ppos){	
    Message *msg;
    char address[16];
    down_interruptible(&mutex);
    msg=(Message*)kmalloc(sizeof(Message), GFP_KERNEL);
    if(copy_from_user( msg , buf, count) ) {		//	Copy data to buffer
        printk("cse536: 5. ERROR: User Fault while copying data to msg");
        return -1;
    }
    memcpy(&(msg->recordID),msg,4);
    memcpy(&(msg->finalClock),&(msg)->recordID+1,4);
    memcpy(&(msg->origClock),&(msg->recordID)+2,4);
    memcpy(&(msg->source),&(msg->recordID)+3,4);
    memcpy(&(msg->destination),&(msg->recordID)+4,4);
    memcpy(&(msg->msgData),&(msg->recordID)+5,236);
    if(msg->recordID){
        push(buf,count,1, destination,msg);
                if(down_timeout(&mutex_rcv, msecs_to_jiffies(5000)) != 0){
                    push(buf,count,1 ,destination,msg);
                    printk("cse536_write - resend ack failed\n");
            }
    }else{
        memcpy(address,&(msg->msgData),16);// set address as 1 indicates ip address which we have set in main program
        destination=in_aton(address);
        printk("cse536_write - setting address: %s\n",address);
    }

    up(&mutex);
    return -1;
}
static long cse536_ioctl(struct file *file,unsigned int cmd,unsigned long arg){
    printk("cse536_ioctl: cmd=%d,arg=%ld\n",cmd,arg);
    return 0;
}
int handler(struct sk_buff *skbuffer){	//hanlder to receive packets based on our protocol number
    Message *msgRcvd;
    uint32_t finalClock;
    struct buffer *p;
    msgRcvd  = (Message*)kmalloc(sizeof(Message), GFP_KERNEL);
    msgRcvd = (Message*)(skbuffer->data);


    switch(msgRcvd->recordID){

    case 0:
        if(LocalClock < msgRcvd->finalClock){
            LocalClock=msgRcvd->finalClock;
        }

        if (msgRcvd->destination == destination ){
           // LocalClock++;

            up(&mutex_rcv);
        }

        break;

    case 1:
        local_origClock++;
        finalClock = update_LocalClock(msgRcvd->finalClock);
        push(msgRcvd->msgData,strlen(msgRcvd->msgData),0,msgRcvd->source,msgRcvd);

        break;

    default:
        printk("cse536: wrong recordID=%d", msgRcvd->recordID);
        break;

    }

    printk("cse536 handler: Record ID %u Final Clock %u Orig Clock %u Message %s\n",(msgRcvd)->recordID,(msgRcvd)->finalClock,(msgRcvd)->origClock,(msgRcvd)->msgData);

    p=(struct buffer *)kmalloc(sizeof(struct buffer),GFP_KERNEL);	//allocate space to store packet in list usign GCP_ATOMIC flag to allocate all the memory or fail
    if(!p){
        printk("Unable to allocate requested Memory\n");
        return 0;
    }
    p->message=msgRcvd;
    if(!bufferhead){// add to the buffer linked list
        bufferhead=p;
    }
    if(buffertail){
        buffertail->next=p;//add new packet to the end of buffer linked list
    }
    buffertail=p;
    p->next=0;
    printk("handler: read packet data: %s\n",(p->message)->msgData);
    return 0;
}
void error(struct sk_buff *skb,__u32 info){
    printk("Error occured in cse536 module %u\n",info);
}
static int __init cse536_init(void){
    int ret;
    struct net_device* device;
    struct in_device* in_dev;
    struct in_ifaddr* if_info;
    device = dev_get_by_name(&init_net,"eth0");
    if(!device){
        printk("Error in getting accessing eth0 address\n");
        return -1;
    }
    in_dev = (struct in_device *)device->ip_ptr;
    if_info = in_dev->ifa_list;
    for (;if_info;if_info=if_info->ifa_next){
        if (!(strcmp(if_info->ifa_label,"eth0")))
        {
            source=if_info->ifa_address;
            printk("if_info->ifa_address = %pI4\n",&source);
            break;
        }
    }
    ret=inet_add_protocol(&new_protocol,PROTOCOL_NUM);// register PROTOCOL_NUM protocol
    if(ret < 0){
        printk("Error while registering new protocol\n");
        return ret;
    }
    ret=register_chrdev(CSE536_MAJOR,"cse5361",&cse536_fops);// register char device
    if(ret < 0){
        printk("Error while registering cse536 device\n");
        return ret;
    }
    LocalClock=0;
    sema_init(&mutex_rcv,0);
    sema_init(&mutex,1);
    printk("cse536: registered module successfully!\n");
    return 0;
}
static void __exit cse536_exit(void){
    inet_del_protocol(&new_protocol,PROTOCOL_NUM);//remove registered PROTOCOL_NUM protocol
    unregister_chrdev(CSE536_MAJOR,"cse5361"); //remove registered char device
    printk("cse536 module Exit\n");
}
struct file_operations cse536_fops={
    owner: THIS_MODULE,
    read: cse536_read,
          write: cse536_write,
          unlocked_ioctl: cse536_ioctl,
          open: cse536_open,
          release: cse536_release,
};
    module_init(cse536_init);
    module_exit(cse536_exit);
    MODULE_AUTHOR("Nagarjuna Myla");
    MODULE_DESCRIPTION("cse536 Module");
    MODULE_LICENSE("GPL");

/*
*   File:       myserver.c
*	Name:		Katayoun Norouzi
*	Email:		knorouzi@ucsc.edu
*	Class:		CMPE156/L - Winter 2016
*	Assign:		Final Project
*
*   Purpose:    This is a skeleton file for a server.
*/

#include "myunp.h"

#include <time.h>

#define MAX_NODES 128

#define MAX_NEIGHBORS 128

#define VALUE_INFINITY 16

int update_rand = 5;
int update_interval=30;
int timeout_interval=180;
int garbage_interval=120;


/**  paramereter  */
unsigned int my_ip=0;
unsigned int my_port=0;
unsigned int table_updated=0;


struct rip_header_type
{
    unsigned char cmd;
    unsigned char version;
    unsigned char zero[2];
};

struct rip_entry_type
{
    unsigned short family;
    unsigned short zero0;
    unsigned int ip;
    unsigned int zero1;
    unsigned int zero2;
    unsigned int distance;
};

struct rip_packet_type
{
    struct rip_header_type header;
    struct rip_entry_type entry[25];
};

struct neighbor_type
{
    unsigned int virtual_ip;
    unsigned int value;
};

struct neighbor_type neighbor[MAX_NEIGHBORS]={};
unsigned int neighbors=0;


#define STATE_FREE      0
#define STATE_ACTIVE    1
#define STATE_TIMEOUT   2


struct entry_type
{
    unsigned int state;
    unsigned int dest_ip;
    unsigned int gateway_ip;
    unsigned int value;
    time_t timeout_time;
    time_t gc_time;
};

struct entry_type node[MAX_NODES]={};


void  updateValue(unsigned int gateway_ip,unsigned int ip,unsigned int value,unsigned int link_value, time_t tm)
{
//    printf("t %08x %08x %d %d\n", gateway_ip, ip, value,link_value);
    int i;
    unsigned int new_value=link_value+value;
    if(new_value>VALUE_INFINITY)
    new_value=VALUE_INFINITY;

    if(my_ip==ip)return;/** I dont need a gateway to myself */

    for(i=0;i<MAX_NODES;i++)
    if(node[i].state && (node[i].dest_ip==ip))
    {
        if(new_value==VALUE_INFINITY)
        {
            /** ony allow current gateway ip node to set route metric to infinite,
            and only reset garbage collection time if its not already infinite */
            if((gateway_ip==node[i].gateway_ip) && (node[i].value!=VALUE_INFINITY))
            {/** force delete value, go to timeout state, ready to be garbage colected */
                node[i].state=STATE_TIMEOUT;
                node[i].value=VALUE_INFINITY;
                node[i].gc_time=tm+garbage_interval;
                table_updated=1;
            }
        }
        else/** always allow updates from gateway ip */
        if((gateway_ip==node[i].gateway_ip) || (node[i].value>=new_value))
        {/** valid route set node to active */

            if(
            (node[i].value>new_value)||(node[i].gateway_ip==gateway_ip)||
            (node[i].timeout_time-tm<timeout_interval/2)
            )
            {/** only update on better value, on update from current gateway,
                or update with same value if current gateway is half or more timed out*/

                if (
                (node[i].value!=new_value)||
                (node[i].gateway_ip!=gateway_ip)
                )
                {/** trigger update only if some value changes */
                    table_updated=1;
                }
                node[i].state=STATE_ACTIVE;
                node[i].gateway_ip=gateway_ip;
                node[i].value=new_value;
                node[i].timeout_time=tm+timeout_interval;/** reset timeout timer */
            }
        }

        return;
    }


    if(new_value==VALUE_INFINITY)
    {/** no need to allocate entry if new one is infinity */
        return;
    }

    for(i=0;i<MAX_NODES;i++)
    if( !node[i].state )
    {
        node[i].dest_ip=ip;
        node[i].gateway_ip=gateway_ip;
        table_updated=1;

        node[i].state=STATE_ACTIVE;
        node[i].value=new_value;
        node[i].timeout_time=tm+timeout_interval;

        return;
    }

}









struct mapping_type
{
    unsigned int virtual_ip;
    unsigned int virtual_port;

    unsigned int physical_ip;
    unsigned int physical_port;
};


struct mapping_type mapping[MAX_NODES];
unsigned int mappings=0;

void addNeighbour(unsigned int ip, unsigned int value)
{
    if(neighbors>=MAX_NEIGHBORS)
    err_quit("too many neighbors\n");

    /**printf("NEIGHBOR %08x %d\n",ip,value);*/

    neighbor[neighbors].virtual_ip=ip;
    neighbor[neighbors].value=value;
    neighbors++;
}

void addMapping(struct mapping_type new_maping)
{
    if(mappings>=MAX_NODES)
    {
        err_quit("Too many mappings !\n");
    }
/*
    printf("%08x %d   %08x %d\n",
           new_maping.physical_ip,new_maping.physical_port,
           new_maping.virtual_ip,new_maping.virtual_port);
*/
    mapping[mappings]=new_maping;

    mappings++;
}



int phyToVirt(struct sockaddr_in * addr)
{
    int i;
    for( i=0;i<mappings;i++)
    if(
       (ntohs(addr->sin_port)==mapping[i].physical_port)&&
       (ntohl(addr->sin_addr.s_addr)==mapping[i].physical_ip)
    )
    {
       addr->sin_port=htons(mapping[i].virtual_port);
       addr->sin_addr.s_addr=htonl(mapping[i].virtual_ip);
       return 0;
    }
    return -1;
}

int virtToPhy(struct sockaddr_in * addr)
{
    int i;
    for(i=0;i<mappings;i++)
    if(
       (ntohs(addr->sin_port)==mapping[i].virtual_port)&&
       (ntohl(addr->sin_addr.s_addr)==mapping[i].virtual_ip)
    )
    {
       addr->sin_port=htons(mapping[i].physical_port);
       addr->sin_addr.s_addr=htonl(mapping[i].physical_ip);
       return 0;
    }
    return -1;
}

void readNodeConfig(const char * filename)
{
    char row[1024];

    FILE *fp=fopen(filename,"r");
    if(!fp)
    err_quit("canot open %s\n",filename);

    while(fgets(row,sizeof(row),fp)!=0)
    {
        int vip[4], vport;
        int pip[4], pport;
        struct mapping_type map;

        if(row[0]=='#')continue;

        sscanf(row,"%u.%u.%u.%u %u %u.%u.%u.%u %u",
               &vip[0],&vip[1],&vip[2],&vip[3],&vport,
               &pip[0],&pip[1],&pip[2],&pip[3],&pport
               );

        map.virtual_ip=(vip[3]<<0)+(vip[2]<<8)+(vip[1]<<16)+(vip[0]<<24);
        map.virtual_port=vport;
        map.physical_ip=(pip[3]<<0)+(pip[2]<<8)+(pip[1]<<16)+(pip[0]<<24);
        map.physical_port=pport;

        addMapping(map);
    }

    fclose(fp);
}


void readNeighborConfig(const char * filename)
{
    char row[1024];

    FILE *fp=fopen(filename,"r");
    if(!fp)
    err_quit("canot open %s\n",filename);

    while(fgets(row,sizeof(row),fp)!=0)
    {
        unsigned int sipb[4],sip;
        unsigned int dipb[4],dip;
        unsigned int v;

        if(row[0]=='#')continue;

        sscanf(row,"%u.%u.%u.%u %u.%u.%u.%u %u",
               &sipb[0],&sipb[1],&sipb[2],&sipb[3],
               &dipb[0],&dipb[1],&dipb[2],&dipb[3],
               &v
               );

        sip=(sipb[3]<<0)+(sipb[2]<<8)+(sipb[1]<<16)+(sipb[0]<<24);
        dip=(dipb[3]<<0)+(dipb[2]<<8)+(dipb[1]<<16)+(dipb[0]<<24);
/*
        printf("%u.%u.%u.%u %u.%u.%u.%u %u\n",
               sipb[0],sipb[1],sipb[2],sipb[3],
               dipb[0],dipb[1],dipb[2],dipb[3],
               v
               );

        printf("%08x %08x %08x\n",sip,dip,my_ip);
*/
        if(sip==my_ip)
        addNeighbour(dip,v);

        if(dip==my_ip)
        addNeighbour(sip,v);

    }

    fclose(fp);
}

int main(int argc, char **argv)
{
    int ip_parts[4],port;

    srand(time(NULL));

    if(argc<3)err_quit("Usage rserver ip port\n");

    if(argc==4)
    {
        update_rand = 2;
        update_interval=3;
        timeout_interval=18;
        garbage_interval=12;
    }

    sscanf(argv[1],"%u.%u.%u.%u",&ip_parts[0],&ip_parts[1],&ip_parts[2],&ip_parts[3]);
    sscanf(argv[2],"%u",&port);

    my_ip=(ip_parts[3]<<0)+(ip_parts[2]<<8)+(ip_parts[1]<<16)+(ip_parts[0]<<24);
    my_port=port;

    readNodeConfig("node.config");
    readNeighborConfig("neighbor.config");


{
    int triggered_update_interval=0;/** seet to random value each time triggered update is requested*/

    int sock= Socket(AF_INET, SOCK_DGRAM, 0);

    time_t next_periodic_time;
    time_t next_trigger_time;

    time_t tstart;

    struct sockaddr_in bind_addr={};
    bind_addr.sin_addr.s_addr=htonl(my_ip);
    bind_addr.sin_port=htons(my_port);

    virtToPhy(&bind_addr);

    bind_addr.sin_addr.s_addr=INADDR_ANY;

    if(-1==Bind(sock,(struct sockaddr*) &bind_addr,sizeof(bind_addr)))
        err_quit("bind failed\n");

    time(&tstart);

    next_periodic_time=tstart;
    next_trigger_time=tstart;

    while(1)
    {
        time_t next_update_time=next_periodic_time;
        int res;
        fd_set fd;

        struct timeval tm={};
        time_t next_event=next_update_time;
        time_t tnow;
        time(&tnow);

        {/** check timeout */
            int i;

            for(i=0;i<MAX_NODES;i++)
            if(node[i].state==STATE_ACTIVE)/** check timeout for each not timed out node */
            {
                if(tnow-node[i].timeout_time>=0)
                {/** node has timed out */
                    node[i].state=STATE_TIMEOUT;
                    node[i].value=VALUE_INFINITY;
                    node[i].gc_time=tnow+garbage_interval;
                    table_updated=1;
                }
            }
            else
            if(node[i].state==STATE_TIMEOUT)
            {/** check garbage collection timeout for timed out nodes*/
                if(tnow-node[i].gc_time>0)
                {/** from timouted state to garbage ollected */
                    node[i].state=STATE_FREE;
                    table_updated=1;
                }
            }
        }

        if(table_updated)
        {
            int i;

            printf("Table for %u.%u.%u.%u %u\n",
                   (my_ip>>24)&0xff,(my_ip>>16)&0xff,
                   (my_ip>> 8)&0xff,(my_ip>> 0)&0xff, my_port);


            for( i=0;i<MAX_NODES;i++)
            if(node[i].state)
            printf("%u.%u.%u.%u %u.%u.%u.%u %u\n",
                   (node[i].dest_ip>>24)&0xff,(node[i].dest_ip>>16)&0xff,
                   (node[i].dest_ip>> 8)&0xff,(node[i].dest_ip>> 0)&0xff,

                   (node[i].gateway_ip>>24)&0xff,(node[i].gateway_ip>>16)&0xff,
                   (node[i].gateway_ip>> 8)&0xff,(node[i].gateway_ip>> 0)&0xff,
                   node[i].value
                   );
            printf("\n");


            if(!triggered_update_interval)
            {/** if no current triggered update pending compute new rand update interval and moment */
                triggered_update_interval=1+(rand()%update_rand);
                next_trigger_time=tnow+triggered_update_interval;
            }

            /** if triggered update is closer than next update*/
            if(next_update_time-next_trigger_time>0)
            next_update_time=tnow+next_trigger_time;

            table_updated=0;/** clear updated flag */
        }

        if(next_update_time-tnow<=0)
        {
            int i,j;

            triggered_update_interval=0;/** clear possible triggered update */

            for(i=0;i<neighbors;i++)
            {
                struct rip_packet_type p={{2,1}};/** command 2 version 2 */

                int entries=0;
                int total=0;

                struct sockaddr_in addr={};
                addr.sin_port=htons(520);
                addr.sin_addr.s_addr=htonl(neighbor[i].virtual_ip);

                if(-1==virtToPhy(&addr))
                err_quit("could not map neighbour %08x : %d",neighbor[i].virtual_ip, 520);


                for(j=0;j<MAX_NODES;j++)
                if(
                   (node[j].state)&&
                   (node[j].dest_ip!=neighbor[i].virtual_ip))/** useless to send a node route to itself */
                {
                    p.entry[entries].ip=node[j].dest_ip;
                    if(node[j].gateway_ip==neighbor[i].virtual_ip)/** do reverse poisoning */
                    p.entry[entries].distance=VALUE_INFINITY;
                    else
                    p.entry[entries].distance=node[j].value;
                    p.entry[entries].family=2;

                    entries++;
                    total++;
                    if(entries==sizeof(p.entry)/sizeof(p.entry[0]))
                    {
                        sendto(sock,&p,sizeof(p.header)+entries*sizeof(p.entry[0]),0,(struct sockaddr*) &addr,sizeof(addr));
                        entries=0;
                    }
                }


                if(entries || (!total))
                /** if nothing is to be send
                we may send at least an empty packet to tell others this node is alive
                this is needed for initial setup,
                else every node would first wait to hear for msome other node before it starts sending packets,
                and it would never hear anyone as everyone is waiting :)*/
                {
                    sendto(sock,&p,sizeof(p.header)+entries*sizeof(p.entry[0]),0,(struct sockaddr*) &addr,sizeof(addr));
                }
            }

            /** update periodic time by adding the interval and a small random value */
            next_periodic_time=tnow+update_interval+(rand()%update_rand);

            continue;
        }


        time(&tnow);

        {
            int i;
            /** find next timeout event time for any entry in the routing table */
            for(i=0;i<MAX_NODES;i++)
            if(node[i].state==STATE_ACTIVE)/** check timeout for each not timed out node */
            {
                if(next_event-node[i].timeout_time>0)
                    next_event=node[i].timeout_time;/** next closest event is timeout of this node*/
            }
            else
            if(node[i].state==STATE_TIMEOUT)
            {
                if(next_event-node[i].gc_time>0)
                    next_event=node[i].gc_time;/** next closest event is garbage collection of this node */
            }
        }

        if((next_event-tnow)<0)
            tm.tv_sec=0;/** the time or next event may have come , dont let tvsec wrap */
        else
            tm.tv_sec=(next_event-tnow);

        FD_ZERO(&fd);
        FD_SET(sock,&fd);
        res = select(sock+1,&fd,0,0,&tm);

        if(res<0)
            err_quit("select error\n");

        if(res==0)continue;/** timeout */

        if(FD_ISSET(sock,&fd))
        {
            int i;
            int link_value = -1;/** metric value for link to this neighbour */
            unsigned int dst_ip;
            struct sockaddr_in addr={};
            socklen_t in_len=sizeof(addr);
            struct rip_packet_type p={};
            res = recvfrom(sock,&p,sizeof(p),0,(struct sockaddr*)&addr,&in_len);

            phyToVirt(&addr);/** get virtual address for this packet */

            dst_ip=ntohl(addr.sin_addr.s_addr);


            if(res<sizeof(p.header))/** either error or some broken packet , ignore it */
                continue;


            if(p.header.cmd!=2)/** only process command 2 */
                continue;

            if((res-sizeof(p.header))%sizeof(p.entry[0]))/** not whole number of entries */
                continue;

            {
                link_value=-1;
                /** find which neighbour is currrent packet from, and get its link value */
                for(i=0;i<neighbors;i++)
                {
                    if(neighbor[i].virtual_ip==dst_ip)
                    link_value=neighbor[i].value;
                }

                if(link_value==-1)continue;/** skip packet  unksnown neighbour*/

                /** update neighbour entry  */
                updateValue(dst_ip,dst_ip,0,link_value,tnow);
            }

            for(i=0;i<(res-sizeof(p.header))/sizeof(p.entry[0]);i++)
            if(p.entry[i].family==2)
            {/** update all received entries  */
                updateValue(dst_ip,p.entry[i].ip,p.entry[i].distance,link_value,tnow);
            }

        }
    }

    Close(sock);
}

    return 0;
}

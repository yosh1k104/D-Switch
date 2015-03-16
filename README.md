D-Switch
==========
This is the ysk's production to graduate, 
Contiki Real-time Extention.

Abstract
----------
In recent years,
developed technology made
sensor nodes cheaper and high performanced.
Along with the advencement of technology,
physical sensors connected to the network
transmit and receive various datas automatically,
and Wireless Sensor Networks (WSN) which use that datas in various ways are popularized.
In particular, the results are remarkable
in environmental monitoring and target tracking.
In the future, WSN will become the foundation of our life supporting.

Operating Systems (OS) in WSN
are divided into events model and threads model.
However, neither one of these models has still fulfilled
all of those requirements
in environments where
real time tasks have been processed
for a long period.
Event models like OS can switch tasks as same as
threads model
by using Protothreads
in Contiki
proposed by Dunkels et al.
However,
if a task which has more priority was posted
while another task has been processed,
the processed task cannot be interrupted,
and cannot be switched to the arrived task
with Protothreads
in the existing circumstances.

In this research, we propose
a dynamically switchable scheduling system for operating systems
using Protothreads
in environments where events with time constraint occurred.
This system enables to save energy by executing tasks
as a general events model,
to trigger interruption, and
preferentially to process tasks
when real time task has been occurred.
Finally, we evaluate this system and prove feasibility.

Real-Time Process Initialization Sample
----------
    PROCESS_THREAD([process_name], ev, data)
    {
        PROCESS_BEGIN();
        
        realtime_task_init(&[process_name], (void (*)(struct rtimer *, void *))[function_name]);

        while(1) {
          PROCESS_YIELD();
        }

        PROCESS_END();
    }

Deployment Method
----------
$ cd \[path\]  
$ sudo chmod 666 /dev/ttyUSB\*  
$ make TARGET=micaz \[file\_name\]  
$ make TARGET=micaz savetarget  
$ make \[file\_name\].upload  PORT=/dev/ttyUSB0 TARGET=micaz  
$ cat /dev/ttyUSB1  

**When you edited hoge.c, \[file\_name\] is hoge.**  


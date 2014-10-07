ROOT_DIR = ../../../..
SAMPL_ROOT = $(ROOT_DIR)/src/net/sampl

ifndef SOX_SUPPORT 
SOX_SUPPORT = 1
endif

ifndef SQLITE_SUPPORT 
SQLITE_SUPPORT = 1
endif


ifeq ($(SOX_SUPPORT),1)
XMPP_LIBRARY_PATH=/home/agr/sensor-andrew/sox/libs/lib_c
include $(XMPP_LIBRARY_PATH)/vars.mk
endif

CFLAGS=-Wall -g $(INCLUDE)
CFLAGS += -D SOX_SUPPORT=$(SOX_SUPPORT)
CFLAGS += -D SQLITE_SUPPORT=$(SQLITE_SUPPORT)


INCLUDE=-I./src/ 

ifdef OPENWRT
CC=/home/soxuser/trunk/staging_dir/toolchain-mipsel_gcc-4.1.2_uClibc-0.9.30.1/usr/bin/mipsel-openwrt-linux-gcc
LIB_INCLUDE=/home/soxuser/trunk/staging_dir/target-mipsel_uClibc-0.9.30.1/usr/include
LIBS=-L/home/soxuser/trunk/staging_dir/target-mipsel_uClibc-0.9.30.1/usr/lib
INCLUDE+=-I$(LIB_INCLUDE)
else
CC=gcc
endif

LIBS+=-lm -lexpat
LDFLAGS+=-L. $(LIBS)

ifeq ($(SOX_SUPPORT),1)
LIBS+=-l$(SOX_LIB_NAME) -lgthread-2.0
LDFLAGS+=-L. $(LIBS)
endif

ifeq ($(SQLITE_SUPPORT),1)
LIBS+=-lsqlite3
endif




ifeq ($(SOX_SUPPORT),1)
INCLUDE+= -I$(GLIB_INCLUDE) 
INCLUDE+= -I$(GLIB_LIB_INCLUDE) 
INCLUDE+= -I./src/xmpp-publish-handlers/  
INCLUDE+= -I./src/xmpp-subscribe-handlers/ 
INCLUDE+= -I$(LOUDMOUTH_INCLUDE) 
INCLUDE+= -I$(XMPP_LIBRARY_PATH) 
endif

ifeq ($(SQLITE_SUPPORT),1)
  INCLUDE+= -I./src/db-write-handlers/  
endif

INCLUDE+= -I$(SAMPL_ROOT)/include/ 
INCLUDE+= -I$(SAMPL_ROOT)/pkt-handlers/ 
INCLUDE+= -I../../transducer-pkts/ 
INCLUDE+= -I./src/calibration/
INCLUDE+= -I$(ROOT_DIR)/tools/SLIPstream/SLIPstream-client/
INCLUDE+=-I../common/


SOURCES=src/main.c 
SOURCES+=../common/pkt_debug.c 
SOURCES+=src/error_log.c 
SOURCES+=$(ROOT_DIR)/tools/SLIPstream/SLIPstream-client/slipstream.c 
SOURCES+=$(SAMPL_ROOT)/include/pkt_packer.c 
SOURCES+=src/slip-server.c 
SOURCES+=src/tx_queue.c 
SOURCES+=src/ffdb.c 
SOURCES+=src/calibration/ff_basic_sensor_cal.c 
SOURCES+=src/calibration/jiga_watt_cal.c 
SOURCES+=$(SAMPL_ROOT)/pkt-handlers/ping_pkt.c 
SOURCES+=$(SAMPL_ROOT)/pkt-handlers/ack_pkt.c 
SOURCES+=$(SAMPL_ROOT)/pkt-handlers/stats_pkt.c
SOURCES+=$(SAMPL_ROOT)/pkt-handlers/neighbor_pkt.c
SOURCES+=$(SAMPL_ROOT)/pkt-handlers/transducer_pkt.c

ifeq ($(SQLITE_SUPPORT),1)
  SOURCES+=src/db-write-handlers/db_transducer.c 
endif

ifeq ($(SOX_SUPPORT),1)
SOURCES+=src/node_cache.c 
SOURCES+=src/xmpp_proxy.c 
SOURCES+=$(SAMPL_ROOT)/pkt-handlers/xmpp_pkt.c 
SOURCES+=src/xmpp-publish-handlers/xmpp_pkt_writer.c 
SOURCES+=src/xmpp-publish-handlers/xmpp_transducer.c 
SOURCES+=src/xmpp-publish-handlers/xmpp_nlist.c 
SOURCES+=src/xmpp-publish-handlers/xmpp_ping.c 
SOURCES+=src/xmpp-subscribe-handlers/composer_handler.c 
endif

SOURCES+=../../transducer-pkts/ff_basic_sensor_pkt.c 
SOURCES+=../../transducer-pkts/ff_power.c 
SOURCES+=src/xml_pkt_parser.c


OBJS=$(SOURCES:.c=.o)

all: gateway_client 

default: gateway_client 

gateway_client: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) -g -c $< -o $@

clean:
	rm -rf *~ $(OBJS) gateway_client

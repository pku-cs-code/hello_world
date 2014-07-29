OBJS_DIR     = objs

OBJS         = $(OBJS_DIR)/main.o           \
			   $(OBJS_DIR)/config.o         \
			   $(OBJS_DIR)/report.o         \
			   $(OBJS_DIR)/sw_scaner.o      \
			   $(OBJS_DIR)/topology.o

IDC_TOPOLOGY_OBJS = $(OBJS_DIR)/config.o  \
			      $(OBJS_DIR)/gen_idc_topology.o
			   

CC           = gcc
CFLAGS       = -g -W -Wall -Werror -Wno-unused-parameter -Wunused-function \
			   -Wunused-variable -Wunused-value -fPIC -I/usr/include/mysql

LIBS         = -lcurl -lutils -lnetsnmp

MAIN_EXE       = sw_scaner
IDC_TOPOLOGY_EXE = gen_idc_topology

main:$(OBJS)
	$(CC) -o $(MAIN_EXE) $(OBJS) $(CFLAGS) $(LIBS)
	@echo $(MAIN_EXE) is generated!

gen:$(IDC_TOPOLOGY_OBJS)
	$(CC) -o $(IDC_TOPOLOGY_EXE) $(IDC_TOPOLOGY_OBJS) $(CFLAGS) $(LIBS) -lmysql
	@echo $(MAIN_EXE) is generated!

$(OBJS_DIR)/main.o:main.c
	$(CC) -c $(CFLAGS) -o $(OBJS_DIR)/main.o main.c 

$(OBJS_DIR)/config.o:config.c
	$(CC) -c $(CFLAGS) -o $(OBJS_DIR)/config.o config.c 

$(OBJS_DIR)/report.o:report.c
	$(CC) -c $(CFLAGS) -o $(OBJS_DIR)/report.o report.c 

$(OBJS_DIR)/sw_scaner.o:sw_scaner.c
	$(CC) -c $(CFLAGS) -o $(OBJS_DIR)/sw_scaner.o sw_scaner.c 

$(OBJS_DIR)/topology.o:topology.c
	$(CC) -c $(CFLAGS) -o $(OBJS_DIR)/topology.o topology.c 

$(OBJS_DIR)/gen_idc_topology.o:gen_idc_topology.c
	$(CC) -c $(CFLAGS) -o $(OBJS_DIR)/gen_idc_topology.o gen_idc_topology.c 

clean:
	rm -f $(MAIN_EXE)
	rm -f $(IDC_TOPOLOGY_EXE)
	rm -f $(OBJS)
	rm -f $(IDC_TOPOLOGY_OBJS)

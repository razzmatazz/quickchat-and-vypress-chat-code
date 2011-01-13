#	Makefile for qcs_link.o: Qchat/Vypress Chat protocol library

qcs_link.o: link.o p_vypress.o p_qchat.o supp.o
	ld -r -o qcs_link.o link.o p_vypress.o p_qchat.o supp.o

supp.o: supp.c supp.h qcs_link.h
	cc -g -c -Wall -o supp.o supp.c
	
link.o: link.c qcs_link.h p_vypress.h p_qchat.h link.h supp.h
	cc -g -c -Wall -o link.o link.c

p_vypress.o: p_vypress.c qcs_link.h p_vypress.h link.h supp.h p_qchat.h
	cc -g -c -Wall -o p_vypress.o p_vypress.c

p_qchat.o: p_qchat.c qcs_link.h p_qchat.h link.h supp.h
	cc -g -c -Wall -o p_qchat.o p_qchat.c

clean:
	rm -f *.o

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>

static void
hex_dump(uint8_t *packet,size_t len, bool pid_all, bool explicit, uint16_t PID)
{
	uint16_t w_pid = (*(packet+1)&0x1F) << 8 | *(packet+2);
	if(!pid_all){
		if(w_pid != PID){
			return;
		}
	}
	if(explicit){
		fprintf(stdout, "sync_byte                   :%02" PRIx8"   ", *packet);
		fprintf(stdout, "transport_error_indicator   :%" PRIx8"\n", (*(packet+1))>>7 & 0x01);
		fprintf(stdout, "payload_unit_start_indicator:%" PRIx8"    ", (*(packet+1))>>6 & 0x01);
		fprintf(stdout, "transport_priority          :%" PRIx8"\n", (*(packet+1))>>5 & 0x01);
		fprintf(stdout, "PID                         :%04" PRIx16" ", (*(packet+1) & 0x1F) << 8 | *(packet+2));
		fprintf(stdout, "transport_scrambling_control:%" PRIx8"\n", (*(packet+3))>>6 & 0x03);
		fprintf(stdout, "adaptation_field_control    :%" PRIx8"    ", (*(packet+3))>>4 & 0x03);
		fprintf(stdout, "continuity_counter          :%" PRIx8" ", (*(packet+3)) & 0x0F);
	}else{
		fprintf(stdout, "[%02" PRIx8" %" PRIx8" %" PRIx8" %" PRIx8" %04" PRIx16" %" PRIx8" %" PRIx8" %" PRIx8"]",
		*packet, (*(packet+1))>>7 & 0x01, (*(packet+1))>>6 & 0x01, (*(packet+1))>>5 & 0x01,
		(*(packet+1) & 0x1F) << 8 | *(packet+2), (*(packet+3))>>6 & 0x03, (*(packet+3))>>4 & 0x03, (*(packet+3)) & 0x0F);
	}
	for(int i=0;i<len;i++){
		if(i%16==0){
			fprintf(stdout, "\n[%3d] ", i);
		}else if(i%8==0){
			fprintf(stdout, "  ");
		}
		fprintf(stdout, "%02" PRIx8" ", *(packet+i));
	}
	fprintf(stdout, "\n");
	return;
}

#define TS_PACKETSIZE 188

int main(int argc, char *argv[])
{
	FILE *fp;
	uint16_t PID = 0x00;
	char *pargv = NULL;
	bool pid_all = false;
	bool explicit = false;

	if(argc==1){
		fprintf(stderr, "ts_dump: ts_dump [-p pid -s] tsfile\n");
		return(1);
	}

	int opt;
	while ((opt = getopt(argc, argv, "sp:")) != -1) {
		switch (opt) {
			case 's':
				explicit = true;
				break;
			case 'p':
				pargv = optarg;
				if(strcmp(pargv, "all")==0){
					pid_all = true;
				}else{
					pid_all = false;
					if(strncmp(pargv, "0x",2)==0 || strncmp(pargv, "0X",2)==0){
						PID = strtoul(pargv+2, NULL, 16);
					}else{
						PID = strtoul(pargv, NULL, 16);
					}
				}
				break;
			default:
				fprintf(stderr, "error! \'%c\' \'%c\'\n", opt, optopt);
				return(1);
		}
	}

	if(argv[optind]==NULL){
		fprintf(stderr, "ts_dump: ts_dump [-p pid -s] tsfile\n");
		return(0);
    }
		
	if((fp=fopen(argv[optind],"r"))==NULL){
		fprintf(stderr, "ts_dump: file open error : %s\n", argv[optind]);
		return(0);
    }

	size_t readlen;
	uint8_t packet[TS_PACKETSIZE];
    while((readlen=fread(packet, TS_PACKETSIZE, 1, fp))==1){
		if(packet[0]!=0x47){
			fprintf(stderr, "ts_dump: file format error : %s\n", argv[optind]);
			break;
		}
		hex_dump(packet, sizeof(packet), pid_all, explicit, PID);
	}
	fclose(fp);

	return(0);
}


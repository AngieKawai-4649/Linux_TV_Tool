#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>


#define print_cas_hex(msg, buf, size) \
	{ \
		uint8_t *w_buf = (uint8_t *)buf; \
		printf("%s 0X", msg ); \
		for(int i=0;i<size;i++){ \
			printf("%02X", *(w_buf+i)); \
		} \
	} \
	printf("\n");

#define byte_order(in,out, size) \
	{ \
		uint8_t *w_in = (uint8_t *)in; \
		uint8_t *w_out = (uint8_t *)out; \
		for(int i=0, j=size-1; i<size; i++,j--){ \
			*(w_out+i) = *(w_in+j); \
		} \
	}

static void card_byte_to_uint32( uint8_t *in, uint32_t *out ){

	uint8_t cd_6[8] = {0,0,0,0,0,0,0,0};
	uint8_t cd_2[2] = {0,0};
	byte_order(in, cd_6, sizeof(cd_6)-2);
	byte_order(in+6, cd_2, sizeof(cd_2));
	uint64_t cd = *((uint64_t *)cd_6) * 100000UL + *(uint16_t *)cd_2;

	*(out+4) = cd%10000UL;
	*(out+3) = cd/10000UL%10000UL;
	*(out+2) = cd/100000000UL%10000UL;
	*(out+1) = cd/1000000000000UL%10000UL;
	*(out) = cd/10000000000000000UL;
}

int main(int argc, char *argv[]){

	uint64_t serialHi;
	uint32_t cardid[5] = {0,0,0,0,0};
	uint8_t cas[8];
	uint16_t check = 0;
	char *cp, *endp;

	if(argc != 2){
		printf("%s card-ID 0000-1111-2222-333 or 0x0123456789ab\n", argv[0]);
		return(0);
	}

	if(strncmp(argv[1], "0x", 2) && strncmp(argv[1], "0X", 2)){

		cp = strtok(argv[1], "-");
		cardid[0] = strtol(cp, &endp, 10);
		if(*endp!='\0'){
			printf("カードIDフォーマットエラー 0000-1111-2222-333 エラー箇所 0 \n");
			return(0);
		}

		// カードID一桁目を強制的に0にする
		cardid[0] %=1000;

		int i;
		for(i=1; i < 4; i++){
			cp = strtok(NULL, "-");
			if(cp == NULL){
				break;
			}
			cardid[i] = strtol(cp, &endp, 10);
			if(*endp!='\0'){
				break;
			}
		}

		if(i != 4){
			printf("カードIDフォーマットエラー 0000-1111-2222-333 エラー箇所 %d \n", i);
			return(0);
		}

		serialHi = cardid[0]*100000000000UL + cardid[1]*10000000UL + cardid[2]*1000UL + cardid[3];

		byte_order(&serialHi,cas, 6);
		
		for(int i=0; i<3; i++){
			check ^= *((uint16_t *)&cas[i*2]);
		}
		memcpy(&cas[6], &check, 2);

		print_cas_hex("CheckCode (2byte)", &check, 2);
		print_cas_hex("cardID (6byte)", cas, 6);
		print_cas_hex("cardID (8byte)", cas, 8);

		card_byte_to_uint32(cas, cardid);
		printf("cardID %04u-%04u-%04u-%04u-%04u\n", cardid[0],cardid[1],cardid[2],cardid[3], cardid[4]);

	}else{
		cp = argv[1]+2;
		if(strlen(cp)!=12){
			printf("カードIDフォーマットエラー 0x0123456789ab %s\n", argv[1]);
			return(0);
		}
		int i;
		bool tf = true;
		for(i=0; i<12; i++){
			if(!isxdigit(*(cp+i))){
				tf = false;
				break;
			}
		}
		if(!tf){
			printf("カードIDフォーマットエラー input HEX 6byte %s\n", argv[1]);
			return(0);
		}

		char buf[3];buf[2]='\0';
		tf = true;
		for(i=0; i<12; i+=2){
			memcpy(buf, cp+i, 2);
			cas[i/2] = strtol(buf, &endp, 16);
			if(*endp!='\0'){
				tf = false;
				break;
			}
		}
		if(!tf){
			printf("カードIDフォーマットエラー 2 input HEX 6byte %s\n", argv[1]);
			return(0);
		}

		for(i=0; i<3; i++){
			check ^= *((uint16_t *)&cas[i*2]);
		}
		memcpy(&cas[6], &check, 2);
		print_cas_hex("CheckCode (2byte)", &check, 2);
		print_cas_hex("cardID (8byte)", cas, 8);

		card_byte_to_uint32(cas, cardid);

		printf("cardID %04u-%04u-%04u-%04u-%04u\n", cardid[0],cardid[1],cardid[2],cardid[3], cardid[4]);
	}

		return(1);
}



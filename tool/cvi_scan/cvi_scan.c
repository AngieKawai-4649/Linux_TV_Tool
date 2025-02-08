#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <stdlib.h>
#include <unistd.h>

#include "libnkf.h"

#define hex_dump(buf,len){ \
	for(int i=0;i<len;i++){ \
		if(i%16==0){ \
			fprintf(stdout, "\n[%4d] ", i); \
		}else if(i%8==0){ \
			fprintf(stdout, "  "); \
		} \
		fprintf(stdout, "%02" PRIx8" ", buf[i]); \
	} \
	fprintf(stdout, "\n"); \
}

// TSヘッダ
typedef struct {
	uint8_t		sync_byte:8;					// 8bit 0x47 固定
	uint8_t		transport_error_indicator:1;	// 1bit
	uint8_t		payload_unit_start_indicator:1;	// 1bit 1:１連のブロックが複数パケットに分割されている際の先頭パケット
												//      0:先頭パケットではない
	uint8_t		transport_priority:1;			// 1bit
	uint16_t	PID:13;							// 13bit
												//	PAT	0x0000
												//	PMT PATによる間接指定
												//	CAT 0x0001
												//	ECM ECM-S PMTによる間接指定
												//	EMM EMM-S CATによる間接指定
												//	NIT 0x0010
												//	SDT 0x0011
												//	BAT 0x0011
												//	EIT 0x0012
												//	EIT ( 地上デジタルテレビジョン放送 ) 0x0012,0x0026,0x0027
												//	RST 0x0013
												//	TDT 0x0014
												//	TOT 0x0014
												//	DCT 0x0017
												//	DLT DCTによる間接指定
												//	DIT 0x001E
												//	SIT 0x001F
												//	LIT 0x0020 またはPMTによる間接指定
												//	ERT 0x0021 またはPMTによる間接指定
												//	ITT PMTによる間接指定
												//	PCAT 0x0022
												//	SDTT 0x0023
												//	SDTT ( 地上デジタルテレビジョン放送 ) 0x0023,0x0028
												//	BIT 0x0024
												//	NBIT 0x0025
												//	LDT 0x0025
												//	CDT 0x0029
												//	多重フレームヘッダ情報 0x002F
												//	DSM-CCセクション PMTによる間接指定
												//	AIT PMTによる間接指定
												//	ST 0x0000,0x0001,0x0014 を除く
												//	ヌルパケット 0x1FFF
	uint8_t		transport_scrambling_control:2;	// 2bit
	uint8_t		adaptation_field_control:2;		// 2bit 01:後続がPayload
												//      10:後続がadaptation field
												//      11:後続がadaptation field + Payload
	uint8_t		continuity_counter:4;			// 4bit パケット連続性チェックカウンター
												//      同一パケットデータにつき0x00〜0x0fまでカウントする
												//      0x0f (15)の次は0に戻る
} TS_HEADER;


// SDT  ServiceDescriptionTable
typedef struct {
		uint8_t		tableId:8;					// 8bit  自トランスポートストリーム 0x42 他トランスポートストリーム 0x46
		uint8_t		sectionSyntaxIndicator:1;	// 1bit  シンタックス指示 常に 1
		uint8_t		reservedFutureUse1:1;		// 1bit  未定義
		uint8_t		reserved1:2;				// 2bit  未定義
		uint16_t	sectionLength:12;			// 12bit セクション長 先頭2bitは常に00
												//		 当フィールド直後からCRC32を含むセクション最後までのbyte数
												//		 仕様上SDT1セクションデータMAX1024byteなのでtableId〜sectionLength
												//		 までの3byteを差し引いた1021byteがsectionLengthの最大値となる
												// ここまで24bit (3byte)
		uint16_t	transportStreamId:16;		// 16bit トランスポート識別
		uint8_t		reserved2:2;				// 2bit  未定義
		uint8_t		versionNumber:5;			// 5bit  バージョン番号
		uint8_t		currentNextIndicator:1;		// 1bit  カレント／ネクスト指示 1: カレント 0:未適用 次のテーブルから有効
		uint8_t		sectionNumber:8;			// 8bit  セクション番号 サブテーブル中の最初のセクションのセクション番号は0x00
												//		 セクション番号は、同一のテーブル識別、トランスポートストリーム識別、
												//		 オリジナルネットワーク識別を持つセクションの追加ごとに 1 加算される。
		uint8_t		lastSectionNumber:8;		// 8bit  サブテーブルの最後のセクション(すなわち、最大のセクション番号を持つセクション)の番号
		uint16_t	originalNetworkId:16;		// 16bit オリジナルネットワーク識別
		uint8_t		reservedFutureUse2:8;		// 8bit  未定義
												// ここまで88 (11byte)

		uint8_t		*sectionData;				// 複数のパケットに存在するセクションデータを蓄積するエリア MAX1021byte
		uint8_t		CRC32[4];
} SDT;


// SdtDescriptor
typedef struct {
		uint16_t	serviceId:16; 				// 16bit サービス識別
		uint8_t		reservedFutureUse:3; 		// 3bit  未定義
		uint8_t		EitUserDefinedFlags:3;		// 3bit  EIT事業者定義フラグ
		uint8_t		EitScheduleFlag:1;  		// 1bit  EIT[対モジュール]フラグ 0:なし 1:あり
		uint8_t		EitPresentFollowingFlag:1;	// 1bit  EIT[現在 /次]フラグ 0:なし 1:あり
		uint8_t		runningStatus:3;			// 3bit  進行状態
		uint8_t		freeCaMode:1;				// 1bit  スクランブル 0:OFF 1:ON
		uint16_t	descriptorsLoopLength:12;	// 12bit 後続記述子の全byte数
												//			例： セクション長245の場合セクション実サイズは233である
												//               -12: -8 11-3, -4: CRC32 4byte
												//				 そこからserviceId〜descriptorsLoopLength まで5byteを
												//				 差し引いた228(byte)が最大値となる
												// ここまで40bit(5byte)
		uint8_t		*descriptor;				// variable size 記述子領域
												// uint8_t[descriptorsLoopLength]
} SdtDescriptor;

// DescriptorX48  サービス記述子
typedef struct {
		uint8_t	descriptorTag:8;				// 8bit
		uint8_t	descriptorLength:8;				// 8bit
		uint8_t	serviceType:8;					// 8bit
		uint8_t	serviceProviderNameLength:8;	// 8bit
		uint8_t	*serviceProviderName;			// uint8_8[serviceProviderNameLength]
		uint8_t	serviceNameLength:8;			// 8bit
		uint8_t	*serviceName;					// uint8_8[serviceNameLength]
} DescriptorX48;

// DescriptorXCB  CA 契約情報記述子(CA contract info descriptor)
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit
		uint8_t		descriptorLength:8;					// uimsbf 8bit
		uint16_t	CaSystemId:16;						// uimsbf 16bit
		uint8_t		CaUnitId:4;							// uimsbf 4bit  0x0:非課金単位グループ
														// 0x1:イベントのデフォルト ES 群を含む課金単位グループ
														// 0x2-0xF:上記以外の課金単位グループ
		uint8_t		numOfComponent:4;					// uimsbf 4bit
		uint8_t		componentTag:8;						// uimsbf byte[numOfComponent]
		uint8_t		contractVerificationInfoLength:8;	// uimsbf 8bit
		uint8_t		*contractVerificationInfo;			// uimsbf byte[contractVerificationInfoLength]
														// 受信機は契約確認情報と視聴予定の年月日情報をIC カードに与え、
														// IC カードが指定された日の視聴可否判定結果を返す。
		uint8_t		feeNameLength:8;					// uimsbf 8bit
		uint8_t		*feeName;							// uimsbf byte[feeNameLength]
														// 記述した ES 群について、その料金を説明する
} DescriptorXCB;

typedef	struct {
		SdtDescriptor	sdesc;
		DescriptorX48	*x48;
		DescriptorXCB	**xCBArray;
	} SDESCARRAY;

typedef struct {
	SDT			sdt;
	 SDESCARRAY	**sdescArray;
} SDTARRAY;

typedef struct {
	SDT				*sdt;
	SdtDescriptor	*sdesc;
	DescriptorX48	*x48;
	DescriptorXCB	**xCBArray;
} XMLOUTPUT;


static SDTARRAY *sdtArraySet(SDTARRAY ***sdtArray, SDT *sdt)
{

	if(*sdtArray == NULL){
		if((*sdtArray = (SDTARRAY **)calloc(1, sizeof(SDTARRAY *)))==NULL){
			return NULL;
		}
		**sdtArray = NULL;
	}

	bool find = false;
	int arrSize;
	for(arrSize=0;*((*sdtArray)+arrSize)!=NULL ;arrSize++){
		if((*((*sdtArray)+arrSize))->sdt.transportStreamId == sdt->transportStreamId){
			find = true;
			break;
		}
	} 
	if(find){
		return(*((*sdtArray)+arrSize));
	}

	SDTARRAY *sdtarr = (SDTARRAY *)calloc(1, sizeof(SDTARRAY ));
	if(sdtarr == NULL){
		return NULL;
	}
	memcpy(&sdtarr->sdt, sdt, sizeof(SDT));
	if((*sdtArray = (SDTARRAY **)realloc(*sdtArray, sizeof(SDTARRAY *) * (arrSize+2)))==NULL){
		return NULL;
	}
	*((*sdtArray)+arrSize)   = sdtarr;
	*((*sdtArray)+arrSize+1) = NULL;
		
	return(*((*sdtArray)+arrSize));
}

static SDESCARRAY *sdescArraySet(SDTARRAY *sdtArray, SdtDescriptor *sdesc)
{

	if(sdtArray->sdescArray == NULL){
		if((sdtArray->sdescArray = (SDESCARRAY **)calloc(1, sizeof(SDESCARRAY *)))==NULL){
			return NULL;
		}
		*sdtArray->sdescArray = NULL;
	}

	bool find = false;
	int arrSize;
	for(arrSize=0;*(sdtArray->sdescArray+arrSize)!=NULL ;arrSize++){
		if((*(sdtArray->sdescArray+arrSize))->sdesc.serviceId == sdesc->serviceId){
			find = true;
			break;
		}
	}
	if(find){
		return(*(sdtArray->sdescArray+arrSize));
	}

	SDESCARRAY *sdescarr = (SDESCARRAY *)calloc(1, sizeof(SDESCARRAY ));
	if(sdescarr == NULL){
		return NULL;
	}
	memcpy(&(sdescarr->sdesc), sdesc, sizeof(SdtDescriptor));
	if((sdtArray->sdescArray = (SDESCARRAY **)realloc(sdtArray->sdescArray, sizeof(SDESCARRAY *) * (arrSize+2)))==NULL){
		return NULL;
	}
	*(sdtArray->sdescArray+arrSize)   = sdescarr;
	*(sdtArray->sdescArray+arrSize+1) = NULL;

	return(*(sdtArray->sdescArray+arrSize));
}

/************************************
 * NKF コマンドパラメータ           *
 *                                  *
 * 入力文字コードの指定             *
 * -J : JIS                         *
 * -E : EUC                         *
 * -S : SJIS                        *
 * -W : UTF-8                       *
 *                                  *
 * 出力文字コードの指定             *
 * -j : JIS                         *
 * -e : EUC                         *
 * -s : SJIS                        *
 * -w : UTF-8                       *
 *                                  *
 * 改行コード変換指定               *
 * -Lu LFに統一                     *
 * -Lw CRLFに統一                   *
 *                                  *
 * 半角全角変換指定                 *
 * -X 半角カナを全角カナにする      *
 * -x 半角カナを全角カナにしない    *
 * -Z 全角を半角にする              *
 * -Z4 全角カナを半角カナにする     *
*************************************/
static DescriptorX48 *x48ArraySet(SDESCARRAY *sdescArray, DescriptorX48 *x48)
{

	if(sdescArray->x48 == NULL){
		if((sdescArray->x48= (DescriptorX48 *)calloc(1, sizeof(DescriptorX48)))==NULL){
			return NULL;
		}
		memcpy(sdescArray->x48, x48, sizeof(DescriptorX48));

		const char *option = "-S -w";
		uint8_t *sjis = aribTOsjis(x48->serviceProviderName, x48->serviceProviderNameLength);
		if(sjis==NULL){
			return NULL;
		}
		if((sdescArray->x48->serviceProviderName = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))==NULL){
			return NULL;
		}
		sdescArray->x48->serviceProviderNameLength = strlen((char *)sdescArray->x48->serviceName);
		free(sjis);

		if((sjis = aribTOsjis(x48->serviceName, x48->serviceNameLength))==NULL){
			return NULL;
		}
		if((sdescArray->x48->serviceName = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))==NULL){
			return NULL;
		}
		sdescArray->x48->serviceNameLength = strlen((char *)sdescArray->x48->serviceName);
		free(sjis);
	}

	return(sdescArray->x48);
}

static DescriptorXCB *xCBArraySet(SDESCARRAY *sdescArray, DescriptorXCB *xCB)
{

	if(sdescArray->xCBArray == NULL){
		if((sdescArray->xCBArray = (DescriptorXCB **)calloc(1, sizeof(DescriptorXCB *)))==NULL){
			return NULL;
		}
		*(sdescArray->xCBArray) = NULL;
	}

	bool find = false;
	int arrSize;
	for(arrSize=0;*(sdescArray->xCBArray+arrSize)!=NULL ;arrSize++){
		if((*(sdescArray->xCBArray+arrSize))->contractVerificationInfoLength == xCB->contractVerificationInfoLength){
			if(!memcmp((*(sdescArray->xCBArray+arrSize))->contractVerificationInfo,xCB->contractVerificationInfo,xCB->contractVerificationInfoLength)){
				find = true;
				break;
			}
		}
	}
	if(find){
		return(*(sdescArray->xCBArray+arrSize));
	}

	DescriptorXCB *xCBArray = (DescriptorXCB *)calloc(1, sizeof(DescriptorXCB));
	if(xCBArray == NULL){
		return NULL;
	}
	memcpy(xCBArray, xCB, sizeof(DescriptorXCB));
	if((xCBArray->contractVerificationInfo = malloc(xCBArray->contractVerificationInfoLength))==NULL){
		return NULL;
	}
	memcpy(xCBArray->contractVerificationInfo, xCB->contractVerificationInfo, xCBArray->contractVerificationInfoLength);

	const char *option = "-S -w";
	uint8_t *sjis = aribTOsjis(xCB->feeName, xCB->feeNameLength);
	if(sjis == NULL){
		return NULL;
	}
	if((xCBArray->feeName = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))==NULL){
		return NULL;
	}
	xCBArray->feeNameLength = strlen((char *)xCBArray->feeName);
	free(sjis);

	if((sdescArray->xCBArray = (DescriptorXCB **)realloc(sdescArray->xCBArray, sizeof(DescriptorXCB *) * (arrSize+2)))==NULL){
		return NULL;
	}
	*(sdescArray->xCBArray+arrSize)   = xCBArray;
	*(sdescArray->xCBArray+arrSize+1) = NULL;

	return(*(sdescArray->xCBArray+arrSize));
}

/*******************************************
static uint32_t table[256];                   /o 下位8ビットに対応する値を入れるテーブル o/

static void
make_crc_table()
{
	uint32_t magic = 0xEDB88320UL;
	/o テーブルを作成する o/
	for (int i = 0; i < 256; i++) {      /o 下位8ビットそれぞれについて計算する o/
		uint32_t table_value = i;      /o 下位8ビットを添え字に、上位24ビットを0に初期化する o/
		for (int j = 0; j < 8; j++) {
			int b = (table_value & 1);   /o 上(反転したので下)から1があふれるかをチェックする o/
			table_value >>= 1;           /o シフトする o/
			if (b) table_value ^= magic; /o 1があふれたらマジックナンバーをXORする o/
		}
		table[i] = table_value;        /o 計算した値をテーブルに格納する o/
	}
}

uint32_t crc32(uint8_t *d, size_t len)
{
	uint32_t crc = 0xFFFFFFFFUL;
	for (size_t i = 0; i<len; i++) {
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ *(d+i)];
	}
	hex_dump(d,len);
	//return ~crc;
	return crc^0xFFFFFFFFUL;
}
******************************************************/


/********************************************/
/* 指定されたPIDをTSファイルから抽出し		*/
/* TS188byteパケットからPID可変長データ		*/
/* (payload)を作り上げる					*/
/********************************************/
static bool
create_payload(uint16_t pid, uint8_t *payload, size_t *payload_len, FILE **fp)
{

#define TS_PACKETSIZE	188
#define MAX_PAYLOAD		4324	// 188 * 23 EIT max size 4096 + stuffing
#define MAX_FILENAME	256
#define SEND_COMMAND	1024
#define RECV_COMMAND	128

	bool		ret = false;
	uint8_t		packet[TS_PACKETSIZE];
	TS_HEADER	ts_header, ts_before;
	size_t		offset = 0;
	int8_t		before_continuity_counter = -1;

	*payload_len = 0;
	memset(&ts_before, '\0', sizeof(TS_HEADER));

	while(fread(packet, TS_PACKETSIZE, 1, *fp)==1){
		ts_header.sync_byte						= packet[0];
		ts_header.transport_error_indicator		= packet[1]>>7 & 0x01;
		ts_header.payload_unit_start_indicator	= packet[1]>>6 & 0x01;
		ts_header.transport_priority			= packet[1]>>5 & 0x01;
		ts_header.PID							= (packet[1] & 0x1F) << 8 | packet[2];
		ts_header.transport_scrambling_control	= packet[3]>>6 & 0x03;
		ts_header.adaptation_field_control		= packet[3]>>4 & 0x03;	// 0b10: adaptation 0b01: payload
		ts_header.continuity_counter			= packet[3] & 0x0F;


		if(ts_header.sync_byte!=0x47){
			ret = false;
			break;
		}

/*****************************
if(ts_header.PID==0x012 && pid==0x012){
		fprintf(stdout, "param pid : %04" PRIx16"\n",pid);
		fprintf(stdout, "sync_byte                    : %02" PRIx8"\n",ts_header.sync_byte);
		fprintf(stdout, "transport_error_indicator    : %" PRIx8"\n",ts_header.transport_error_indicator);
		fprintf(stdout, "payload_unit_start_indicator : %" PRIx8"\n",ts_header.payload_unit_start_indicator);
		fprintf(stdout, "transport_priority           : %" PRIx8"\n",ts_header.transport_priority);
		fprintf(stdout, "PID                          : %04" PRIx16"\n",ts_header.PID);
		fprintf(stdout, "transport_scrambling_control : %" PRIx8"\n",ts_header.transport_scrambling_control);
		fprintf(stdout, "adaptation_field_control     : %" PRIx8"\n",ts_header.adaptation_field_control);
		fprintf(stdout, "continuity_counter           : %" PRIx8"\n",ts_header.continuity_counter);
		hex_dump(packet,188);
}
****************************/

		/****************************************************************************************************/
		/* adaptation field:0b01 payload 			payload_unit_start_indicator が1の時payload 先頭		*/
		/*											packet[4]はpointer field となり payload データスタート	*/
		/*											位置が入っている										*/
		/*											通常は0 この場合はpacket[5]からpayload data となる		*/
		/*											payload_unit_start_indicator が 0 の時payload 後続		*/
		/*											packet[4]からpayload data となる(pointer field 無し)	*/
		/* 					0b10 adaptation field   payload無しなのでこのパケットは読み飛ばす				*/
		/*					0b11 adaptation field & payload packet[4] にadaptation field byte数が入っている	*/
		/*											payload はそのbyte数シフトした位置から始まる			*/
		/* 例：adaptation_field_control:0b01 and payload_unit_start_indicator:0b00 の時、					*/
		/*      188packet: TSheader 4byte + 184byte payload となる（余ったエリアは0xFFで埋められる）		*/
		/* 例：adaptation_field_control:0b11 の時															*/
		/*      payload packet[4]に設定されたポインタが0xA0とするとpacket[4+1+0xA0]からpacket[187]までが	*/
		/*		payloadとなる(4:TS HEADER 1:pointer [5]〜[4+A0]adaptation field [4+1+A0]〜[187] payload)	*/
		/****************************************************************************************************/

		if(ts_header.PID==pid){
			offset = 4;	// TS HEADER 4byte

			// 後続がアダプテーションフィールドのみ
			// 読み飛ばす
			if(ts_header.adaptation_field_control == 0b10){
				*payload_len = 0;
				continue;
			}

/***
			// TSヘッダ後続がアダプテーションフィールド&ペイロード
			if(ts_header.adaptation_field_control==0b11){
				// TSヘッダ直後の1byteをadaptation field lengthとして
				// adaptation field length 1byte + 中にセットされてる
				// データサイズ分offsetをカウントアップし読み飛ばす
				offset += (packet[offset]+1);
				// 異常データの場合は蓄積したpayloadを破棄して読み飛ばす
				if(offset>=TS_PACKETSIZE){
					*payload_len = 0;
					continue;
				}
			}
***/

			// TSヘッダ後続がアダプテーションフィールド&ペイロード またはペイロードのみ
			if(ts_header.adaptation_field_control & 0b01){
				// ペイロード先頭
				if(ts_header.payload_unit_start_indicator==1){
					// packet[4]はポインタフィールド ペイロード先頭位置を指すbyte数が入っている
					if(*payload_len==0){
						offset += packet[4];
						offset += 1;
						if(*payload_len + TS_PACKETSIZE-offset < MAX_PAYLOAD){
							memcpy(payload, &packet[offset], TS_PACKETSIZE-offset);
							*payload_len = TS_PACKETSIZE-offset;
						}else{
							// 異常データの場合は蓄積したpayloadを破棄して作り直す
							*payload_len = 0;
							continue;
						}
					// 次のpayload先頭を読み込んだので既にpayloadは作成済み
					}else{
						// TSヘッダ後続がペイロードのみ
						if(ts_header.adaptation_field_control == 0b01){
							// packet[4]に1以上の値がセットされている場合、
							// packet[5]からpacket[4]にセットされているbyte数をpayloadに追加する
							if(packet[4]!=0x00){
								offset += 1;
								if(*payload_len + packet[4] < MAX_PAYLOAD){
									memcpy(payload+(*payload_len), &packet[offset], packet[4]);
									*payload_len += packet[4];
								}else{
									// 異常データの場合は蓄積したpayloadを破棄して作り直す
									*payload_len = 0;
									continue;
								}
								
							}
						}
						// 読み込んだTSパケット(188byte)ファイルポインタを戻して終了
						fseek(*fp, -TS_PACKETSIZE, SEEK_CUR);
						ret = true;
						break;
					}
				// ペイロード後続
				}else{
					// まだペイロード先頭データを取り込んでいないので読み飛ばす
					if(*payload_len==0){
						continue;
					}
					if(ts_header.continuity_counter==before_continuity_counter){
						if(*payload_len + TS_PACKETSIZE-offset < MAX_PAYLOAD){
							memcpy(payload+(*payload_len), &packet[offset], TS_PACKETSIZE-offset);
							*payload_len += TS_PACKETSIZE-offset;
						}else{
						// 異常データの場合は蓄積したpayloadを破棄して作り直す
							*payload_len = 0;
							continue;
						}
					// drop した場合データを破棄して作り直す
					}else{
						*payload_len = 0;
						continue;
					}
				}
				before_continuity_counter = (ts_header.continuity_counter==0x0f)?0:ts_header.continuity_counter+1;
			}
		}
	}
	return(ret);
}

static void
SDT_set(uint8_t *payload, SDT *sdt)
{
	sdt->tableId = *payload;
	sdt->sectionSyntaxIndicator = *(payload+1)>>7 & 0x01;
	sdt->reservedFutureUse1		= *(payload+1)>>6 & 0x01;
	sdt->reserved1				= *(payload+1)>>4 & 0x03;
	sdt->sectionLength			= (*(payload+1)&0x0f)<<8 | *(payload+2);
	sdt->transportStreamId		= (*(payload+3)&0xff)<<8 | *(payload+4);
	sdt->reserved2				= *(payload+5)>>6 & 0x03;
	sdt->versionNumber			= *(payload+5)>>1 & 0x1f;
	sdt->currentNextIndicator	= *(payload+5)>>1 & 0x01;
	sdt->sectionNumber			= *(payload+6);
	sdt->lastSectionNumber		= *(payload+7);
	sdt->originalNetworkId		= (*(payload+8)&0xff)<<8 | *(payload+9);
	sdt->reservedFutureUse2		= *(payload+10);
	sdt->sectionData			= payload+11;
	memcpy(sdt->CRC32, payload+3+sdt->sectionLength-4, 4);

	return;
}

static void
SdtDescriptor_set(uint8_t *sectionData, SdtDescriptor *sdesc)
{
	sdesc->serviceId					= (*sectionData&0xff)<<8 | *(sectionData+1);
	sdesc->reservedFutureUse			= *(sectionData+2)>>5 & 0x07;
	sdesc->EitUserDefinedFlags			= *(sectionData+2)>>2 & 0x07;
	sdesc->EitScheduleFlag				= *(sectionData+2)>>1 & 0x01;
	sdesc->EitPresentFollowingFlag		= *(sectionData+2) & 0x01;
	sdesc->runningStatus				= *(sectionData+3)>>5 & 0x07;
	sdesc->freeCaMode					= *(sectionData+3)>>4 & 0x01;
	sdesc->descriptorsLoopLength		= (*(sectionData+3)&0x0f)<<8 | *(sectionData+4);
	sdesc->descriptor					= sectionData+5;

	return;
}

static void
DescriptorX48_set(uint8_t *descriptor, DescriptorX48 *x48)
{
	x48->descriptorTag				= *descriptor;
	x48->descriptorLength			= *(descriptor+1);
	x48->serviceType				= *(descriptor+2);
	x48->serviceProviderNameLength	= *(descriptor+3);
	x48->serviceProviderName		= descriptor+4;
	x48->serviceNameLength			= *(descriptor+4+x48->serviceProviderNameLength);
	x48->serviceName				= descriptor+4+x48->serviceProviderNameLength+1;

	return;
}

static void
DescriptorXCB_set(uint8_t *descriptor, DescriptorXCB *xCB)
{
	xCB->descriptorTag					= *descriptor;
	xCB->descriptorLength				= *(descriptor+1);
	xCB->CaSystemId						= (*(descriptor+2)&0xff)<<8 | *(descriptor+3);
	xCB->CaUnitId						= *(descriptor+4)>>4&0x0f;
	xCB->numOfComponent					= *(descriptor+4)&0x0f;
	xCB->componentTag					= *(descriptor+5);
	xCB->contractVerificationInfoLength	= *(descriptor+5+xCB->numOfComponent);
	xCB->contractVerificationInfo		= descriptor+5+xCB->numOfComponent+1;
	xCB->feeNameLength					= *(descriptor+5+xCB->numOfComponent+1+xCB->contractVerificationInfoLength);
	xCB->feeName						= descriptor+5+xCB->numOfComponent+1+xCB->contractVerificationInfoLength+1;

	return;
} 

static int compare_tsid(const void *a, const void *b)
{
	return((*(XMLOUTPUT **)a)->sdt->transportStreamId - (*(XMLOUTPUT **)b)->sdt->transportStreamId);
}

static int compare_sid(const void *a, const void *b)
{
	return((*(XMLOUTPUT **)a)->sdesc->serviceId - (*(XMLOUTPUT **)b)->sdesc->serviceId);
}

static int compare_tsid_sid(const void *a, const void *b)
{
	int rtn;
	if((rtn=compare_tsid(a,b))==0){
		return(compare_sid(a,b));
	}else{
		return(rtn);
	}

}

static void sort_tsid_sid(XMLOUTPUT **a, size_t size)
{
	qsort(a,size,sizeof(XMLOUTPUT *),compare_sid);
	qsort(a,size,sizeof(XMLOUTPUT *),compare_tsid);
	qsort(a,size,sizeof(XMLOUTPUT *),compare_tsid_sid);
}

static void printSDT(SDT *sdt)
{
	fprintf(stdout, "tableId                : %02" PRIx8"\n",  sdt->tableId);
	fprintf(stdout, "sectionSyntaxIndicator : %" PRIx8"\n",    sdt->sectionSyntaxIndicator);
	fprintf(stdout, "reservedFutureUse1     : %" PRIx8"\n",    sdt->reservedFutureUse1);
	fprintf(stdout, "reserved1              : %02" PRIx8"\n",  sdt->reserved1);
	fprintf(stdout, "sectionLength          : %04" PRIx16"\n", sdt->sectionLength);
	fprintf(stdout, "transportStreamId      : %04" PRIx16"\n", sdt->transportStreamId);
	fprintf(stdout, "reserved2              : %02" PRIx8"\n",  sdt->reserved2);
	fprintf(stdout, "versionNumber          : %02" PRIx8"\n",  sdt->versionNumber);
	fprintf(stdout, "currentNextIndicator   : %" PRIx8"\n",    sdt->currentNextIndicator);
	fprintf(stdout, "sectionNumber          : %02" PRIx8"\n",  sdt->sectionNumber);
	fprintf(stdout, "lastSectionNumber      : %02" PRIx8"\n",  sdt->lastSectionNumber);
	fprintf(stdout, "originalNetworkId     : %04" PRIx16"\n",  sdt->originalNetworkId);
	fprintf(stdout, "reservedFutureUse2     : %02" PRIx8"\n",  sdt->reservedFutureUse2);
	fprintf(stdout, "sectionData            : \n");
	//hex_dump(sdt->sectionData,sdt->sectionLength-8-4);
	fprintf(stdout, "CRC32                  : ");
	hex_dump(sdt->CRC32,4);

	return;
}

static void printSdtDescriptor(SdtDescriptor *sdesc)
{
	fprintf(stdout, "\tserviceId               : %04" PRIx16"\n", sdesc->serviceId);
	fprintf(stdout, "\treservedFutureUse       : %02" PRIx8"\n",  sdesc->reservedFutureUse);
	fprintf(stdout, "\tEitUserDefinedFlags     : %02" PRIx8"\n",  sdesc->EitUserDefinedFlags);
	fprintf(stdout, "\tEitScheduleFlag         : %" PRIx8"\n",    sdesc->EitScheduleFlag);
	fprintf(stdout, "\tEitPresentFollowingFlag : %" PRIx8"\n",    sdesc->EitPresentFollowingFlag);
	fprintf(stdout, "\trunningStatus           : %02" PRIx8"\n",  sdesc->runningStatus);
	fprintf(stdout, "\tfreeCaMode              : %" PRIx8"\n",    sdesc->freeCaMode);
	fprintf(stdout, "\tdescriptorsLoopLength   : %04" PRIx16"\n", sdesc->descriptorsLoopLength);

	fprintf(stdout, "\tdescriptor              : \n");
	//hex_dump(sdesc->descriptor,sdesc->descriptorsLoopLength);

	return;
}

static void printDescriptorX48(DescriptorX48 *x48)
{
	fprintf(stdout, "\t\tdescriptorTag             : %02" PRIx8"\n",  x48->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength          : %02" PRIx8"\n",  x48->descriptorLength);
	fprintf(stdout, "\t\tserviceType               : %02" PRIx8"\n",  x48->serviceType);
	fprintf(stdout, "\t\tserviceProviderNameLength : %02" PRIx8"\n",  x48->serviceProviderNameLength);
	fprintf(stdout, "\t\tserviceProviderName       : ");

	fprintf(stdout, "%s\n", x48->serviceProviderName);
//	hex_dump(x48->serviceProviderName,x48->serviceProviderNameLength);
	fprintf(stdout, "\t\tserviceNameLength         : %02" PRIx8"\n",  x48->serviceNameLength);
	fprintf(stdout, "\t\tserviceName               : ");
	fprintf(stdout, "%s\n", x48->serviceName);
//	hex_dump(x48->serviceName,x48->serviceNameLength);

	return;
}

static void printDescriptorXCB(DescriptorXCB *xCB)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xCB->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8"\n",  xCB->descriptorLength);
	fprintf(stdout, "\t\tCaSystemId                     : %04" PRIx16"\n", xCB->CaSystemId);
	fprintf(stdout, "\t\tCaUnitId                       : %02" PRIx8"\n",  xCB->CaUnitId);
	fprintf(stdout, "\t\tnumOfComponent                 : %02" PRIx8"\n",  xCB->numOfComponent);
	fprintf(stdout, "\t\tcomponentTag                   : %02" PRIx8"\n",  xCB->componentTag);
	fprintf(stdout, "\t\tcontractVerificationInfoLength : %02" PRIx8"\n",  xCB->contractVerificationInfoLength);
	fprintf(stdout, "\t\tcontractVerificationInfo       : ");
	hex_dump(xCB->contractVerificationInfo,xCB->contractVerificationInfoLength);
	fprintf(stdout, "\t\tfeeNameLength                  : %02" PRIx8"\n",  xCB->feeNameLength);
	fprintf(stdout, "\t\tfeeName                        : ");
	fprintf(stdout, "%s\n", xCB->feeName);
//	hex_dump(xCB->feeName,xCB->feeNameLength);

	return;
}

static void printInfo(XMLOUTPUT **xml, size_t xmlSize )
{
	uint16_t transportStreamId = 0;
	for(size_t i=0;i<xmlSize; i++){
		if(transportStreamId != (*(xml+i))->sdt->transportStreamId){
			fprintf(stdout, "********************************************************\n");
			printSDT((*(xml+i))->sdt);
			transportStreamId = (*(xml+i))->sdt->transportStreamId;
		}
		fprintf(stdout, "\n");
		printSdtDescriptor((*(xml+i))->sdesc);
		fprintf(stdout, "\n");

		if((*(xml+i))->x48 != NULL){
			printDescriptorX48((*(xml+i))->x48);
			fprintf(stdout, "\n");
		}
		if((*(xml+i))->xCBArray != NULL){
			for(int f=0; *((*(xml+i))->xCBArray+f) != NULL; f++){
				printDescriptorXCB(*((*(xml+i))->xCBArray+f));
				fprintf(stdout, "\n");
			}
		}
	}
	return;
}

static void	printXmlPart(XMLOUTPUT *xml)
{
	if(xml->sdesc->freeCaMode == 1){
		fprintf(stdout, "\t<ServiceID sid=\"%d\" tsid=\"%d\" nid=\"%d\" name=\"%s\" >\n",
				xml->sdesc->serviceId,
				xml->sdt->transportStreamId,
				xml->sdt->originalNetworkId,
				xml->x48->serviceName);
		fprintf(stdout, "\t\t<cvi>\n\t\t\t");
		if(xml->xCBArray !=NULL){
			DescriptorXCB *xcbWork = *(xml->xCBArray);
			for(size_t i=0;i<xcbWork->contractVerificationInfoLength;i++){
				fprintf(stdout, "%02" PRIX8"", *(xcbWork->contractVerificationInfo+i));
			}
			fprintf(stdout, "\n\t\t</cvi>\n");
		}
		fprintf(stdout, "\t</ServiceID>\n");
	}
}

static void printXml(XMLOUTPUT **xmlBS, size_t xmlBSSize,XMLOUTPUT **xmlCS, size_t xmlCSSize)
{
	fprintf(stdout, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n");
	fprintf(stdout, "<CVI_INFOMATION>\n\n");

	for(size_t i=0; i<xmlBSSize; i++){
		printXmlPart(*(xmlBS+i));
	}
	for(size_t i=0; i<xmlCSSize; i++){
		printXmlPart(*(xmlCS+i));
	}
	fprintf(stdout, "</CVI_INFOMATION>\n\n");
}


int main(int argc, char *argv[])
{

	uint8_t xmlFlag = 0;
	uint8_t tsfileNum;
	int		opt;

	uint8_t payload[MAX_PAYLOAD];
	size_t	payload_len;
	FILE *fp;
	SDT	sdt;
	SdtDescriptor	sdesc;
	DescriptorX48	x48;
	DescriptorXCB	xCB;

	SDTARRAY 	**sdtArray  = NULL;
	SDTARRAY 	*sdtCurrent = NULL;
	SDESCARRAY	*sdescCurrent = NULL;

	while ((opt = getopt(argc, argv, "x")) != -1){
		switch (opt) {
			case 'x':
				xmlFlag = 1;
			break;
			default:
				fprintf(stderr, "Usage: %s [-x] TSfile1 TSfile2 ...\n", argv[0]);
				return(-1);
		}
	}

	if(xmlFlag){
		if(optind >= argc || argc > 5){
			fprintf(stderr, "Usage: %s [-x] TSfile1 TSfile2 ...\n", argv[0]);
			return(-1);
		}
	}else{
		if(argc > 4){
			fprintf(stderr, "Usage: %s [-x] TSfile1 TSfile2 ...\n", argv[0]);
			return(-1);
		}
	}

	tsfileNum = argc - optind;
	for(uint8_t i = 0; i<tsfileNum; i++){
		if((fp=fopen(argv[optind+i],"r"))==NULL){
			fprintf(stderr, "file open error : %s\n", argv[optind+i]);
			return(-1);
   		 }

		bool find = false;

		while(!feof(fp)){
			find = create_payload(0x0011, payload, &payload_len, &fp);	/* 0x11 SDT	*/
			if(find){
				memset(&sdt, '\0', sizeof(SDT));
				SDT_set(payload, &sdt);
				sdtCurrent = sdtArraySet(&sdtArray, &sdt);
				for(int sDescriptorLength=0; sDescriptorLength<sdt.sectionLength-8-4; sDescriptorLength+=5+sdesc.descriptorsLoopLength){
																				// 8 : SDT transportStreamId から reservedFutureUse2 までのbyte数
																				// 4 : SDT CRC32 のbyte数
																				// sdt.sectionLength-8-4 : SDT sectionData のbyte数
																				// 5 : serviceId から descriptorsLoopLength までのbyte数
																				// sdesc.descriptorsLoopLength : descriptor のbyte数
																				// 5+sdesc.descriptorsLoopLength : SDT Descriptor 1つのbyte数
					memset(&sdesc, '\0', sizeof(SdtDescriptor));
					SdtDescriptor_set(sdt.sectionData+sDescriptorLength, &sdesc);
					sdescCurrent = sdescArraySet(sdtCurrent, &sdesc);
					uint8_t descriptorTag;
					for(int descriptorOffset=0; descriptorOffset<sdesc.descriptorsLoopLength; descriptorOffset+=*(sdesc.descriptor+descriptorOffset+1) + 2){
																				// *(sdesc.descriptor+descriptorOffset+1) : descriptorLength
																				// 2 : descriptorTag 1byte descriptorLength 1byte
																				// *(sdesc.descriptor+descriptorOffset+1) + 2はdescriptor1つのbyte数
						descriptorTag = *(sdesc.descriptor+descriptorOffset);
						if(descriptorTag==0x48){		// 0x48:サービス記述子
							memset(&x48, '\0', sizeof(DescriptorX48));
							DescriptorX48_set(sdesc.descriptor+descriptorOffset, &x48);
							x48ArraySet(sdescCurrent, &x48);
						}else if(descriptorTag==0xcb){	// 0xcb:CA 契約情報記述子
							memset(&xCB, '\0', sizeof(DescriptorXCB));
							DescriptorXCB_set(sdesc.descriptor+descriptorOffset, &xCB);
							xCBArraySet(sdescCurrent, &xCB);
						}

					}
				}
			}
		}
		fclose(fp);
	}

	XMLOUTPUT	**xmlBS= NULL;
	XMLOUTPUT	**xmlCS= NULL;
	size_t xmlBSSize = 0, xmlCSSize = 0;

	if(sdtArray == NULL){
		fprintf(stdout, "SDT Not Found\n");
	}else{
		for(size_t i=0; *(sdtArray+i) != NULL; i++){
			SDESCARRAY	**sdescWork = (*(sdtArray+i))->sdescArray;
			for(size_t k=0; *(sdescWork+k) != NULL; k++){
				XMLOUTPUT *xmlWork = (XMLOUTPUT *)calloc(1,sizeof(XMLOUTPUT));
				xmlWork->sdt = &((*(sdtArray+i))->sdt);
				xmlWork->sdesc = &((*(sdescWork+k))->sdesc);
				xmlWork->x48 = (*(sdescWork+k))->x48;
				xmlWork->xCBArray = (*(sdescWork+k))->xCBArray;

				if((*(sdtArray+i))->sdt.originalNetworkId == 4){	// BS
					xmlBS = (XMLOUTPUT **)realloc(xmlBS, sizeof(XMLOUTPUT *) * (xmlBSSize+1));
					*(xmlBS+xmlBSSize) = xmlWork;
					xmlBSSize++;
				}else if((*(sdtArray+i))->sdt.originalNetworkId == 6 || (*(sdtArray+i))->sdt.originalNetworkId == 7){	// CS
					xmlCS = (XMLOUTPUT **)realloc(xmlCS, sizeof(XMLOUTPUT *) * (xmlCSSize+1));
					*(xmlCS+xmlCSSize) = xmlWork;
					xmlCSSize++;
				}
			}
		}

		if(xmlFlag == 1){
			qsort(xmlBS, xmlBSSize, sizeof(XMLOUTPUT *), compare_sid);
			qsort(xmlCS, xmlCSSize, sizeof(XMLOUTPUT *), compare_sid);
			printXml(xmlBS, xmlBSSize,xmlCS, xmlCSSize);
		}else{
			sort_tsid_sid(xmlBS, xmlBSSize);
			sort_tsid_sid(xmlCS, xmlCSSize);
			printInfo(xmlBS, xmlBSSize);
			printInfo(xmlCS, xmlCSSize);
		}

		/* アロケートメモリ解放 */
		for(size_t i=0; i<xmlBSSize;i++){
			free(*(xmlBS+i));
		}
		free(xmlBS);
		for(size_t i=0; i<xmlCSSize;i++){
			free(*(xmlCS+i));
		}
		free(xmlCS);
		for(size_t i=0; *(sdtArray+i) != NULL; i++){
			SDESCARRAY	**sdescWork = (*(sdtArray+i))->sdescArray;
			for(size_t k=0; *(sdescWork+k) != NULL; k++){
				free((*(sdescWork+k))->x48->serviceProviderName);
				free((*(sdescWork+k))->x48->serviceName);
				free((*(sdescWork+k))->x48);
				if((*(sdescWork+k))->xCBArray != NULL){
					for(size_t g=0; *((*(sdescWork+k))->xCBArray+g) != NULL; g++){
						free((*((*(sdescWork+k))->xCBArray+g))->contractVerificationInfo);
						free((*((*(sdescWork+k))->xCBArray+g))->feeName);
						free(*((*(sdescWork+k))->xCBArray+g));
					}
				}
				free((*(sdescWork+k))->xCBArray);
				free(*(sdescWork+k));
			}
			free(sdescWork);
			free(*(sdtArray+i));
		}
		free(sdtArray);
	}

	return(0);
}


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ctype.h>
#include <getopt.h>

#include <stdlib.h>
#include <unistd.h>

#include <time.h>

#include "libnkf.h"

#define hex_dump(buf,len, tab){ \
	char t[10]; \
	for(int i=0; i<tab; i++){ \
		t[i] = '\t'; \
	} \
	t[tab] = '\0'; \
	for(int i=0;i<len;i++){ \
		if(i%16==0){ \
			fprintf(stdout, "\n%s[%4d] ",t, i); \
		}else if(i%8==0){ \
			fprintf(stdout, "  "); \
		} \
		fprintf(stdout, "%02" PRIx8" ", buf[i]); \
	} \
	fprintf(stdout, "\n"); \
}


// 実行時オプションパラメータ格納
typedef struct {
	uint16_t	pid;
	uint16_t	sid;
	char		*file;
} ARG_PARAM;

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

// EIT  Event Information Table
typedef struct {
		uint8_t		tableId:8;					// 8bit  0x4E : 自ストリームの現在と次の番組
												//		 0x4F : 他ストリームの現在と次の番組
												//		 0x50 : 自ストリームの本日〜 4日の番組
												//		 0x51 : 自ストリームの 5日〜 8日の番組
												//		 0x52 : 自ストリームの 9日〜12日の番組
												//		 0x53 : 自ストリームの13日〜16日の番組
												//		 0x54 : 自ストリームの17日〜20日の番組
												//		 0x55 : 自ストリームの21日〜24日の番組
												//		 0x56 : 自ストリームの25日〜28日の番組
												//		 0x57 : 自ストリームの29日〜32日の番組
												//		 0x60 : 他ストリームの本日〜 4日の番組
												//		 0x61 : 他ストリームの 5日〜 8日の番組
												//		 0x62 : 他ストリームの 9日〜12日の番組
												//		 0x63 : 他ストリームの13日〜16日の番組
												//		 0x64 : 他ストリームの17日〜20日の番組
												//		 0x65 : 他ストリームの21日〜24日の番組
												//		 0x66 : 他ストリームの25日〜28日の番組
												//		 0x67 : 他ストリームの29日〜32日の番組
		uint8_t		sectionSyntaxIndicator:1;	// 1bit  シンタックス指示 常に 1
		uint8_t		reservedFutureUse1:1;		// 1bit  未定義
		uint8_t		reserved1:2;				// 2bit  未定義
		uint16_t	sectionLength:12;			// 12bit EITペイロードMAX4096byteなのでセクション長=4096-ここまでのbyte数3byte=4093byte以下となる
												// ここまで24 (3byte)

		uint16_t	serviceId:16;				// 16bit サービス識別
		uint8_t		reserved2:2;				// 2bit  未定義
		uint8_t		versionNumber:5;			// 5bit  バージョン番号
		uint8_t		currentNextIndicator:1;		// 1bit  カレント／ネクスト指示 1: カレント 0:未適用 次のテーブルから有効
		uint8_t		sectionNumber:8;			// 8bit  1日を3時間毎に区切り、0時〜3時、3時〜6時、 ・・・、21 時〜24 時のそれぞれの時間毎(segment) に
												//       最大 8section のテーブルを用いて番組情報を記述する。
												//       １つのテーブルID毎に4日間を0X00〜0XFFで割り振る
												//       8section全てを使用しない場合、section_numberに抜けができても良い
												// 1日目 0:00:00〜 2:59:59 : 0X00〜0X07
												// 1日目 3:00:00〜 5:59:59 : 0X08〜0X0F
												// 1日目 6:00:00〜 8:59:59 : 0X10〜0X17
												// 1日目 9:00:00〜11:59:59 : 0X18〜0X1F
												// 1日目12:00:00〜14:59:59 : 0X20〜0X27
												// 1日目15:00:00〜17:59:59 : 0X28〜0X2F
												// 1日目18:00:00〜20:59:59 : 0X30〜0X37
												// 1日目21:00:00〜23:59:59 : 0X38〜0X3F
												// 2日目 0:00:00〜 2:59:59 : 0X40〜0X47
												// 2日目 3:00:00〜 5:59:59 : 0X48〜0X4F
												// 2日目 6:00:00〜 8:59:59 : 0X50〜0X57
												// 2日目 9:00:00〜11:59:59 : 0X58〜0X5F
												// 2日目12:00:00〜14:59:59 : 0X60〜0X67
												// 2日目15:00:00〜17:59:59 : 0X68〜0X6F
												// 2日目18:00:00〜20:59:59 : 0X70〜0X77
												// 2日目21:00:00〜23:59:59 : 0X78〜0X7F
												// 3日目 0:00:00〜 2:59:59 : 0X80〜0X87
												// 3日目 3:00:00〜 5:59:59 : 0X88〜0X8F
												// 3日目 6:00:00〜 8:59:59 : 0X90〜0X97
												// 3日目 9:00:00〜11:59:59 : 0X98〜0X9F
												// 3日目12:00:00〜14:59:59 : 0XA0〜0XA7
												// 3日目15:00:00〜17:59:59 : 0XA8〜0XAF
												// 3日目18:00:00〜20:59:59 : 0XB0〜0XB7
												// 3日目21:00:00〜23:59:59 : 0XB8〜0XBF
												// 4日目 0:00:00〜 2:59:59 : 0XC0〜0XC7
												// 4日目 3:00:00〜 5:59:59 : 0XC8〜0XCF
												// 4日目 6:00:00〜 8:59:59 : 0XD0〜0XD7
												// 4日目 9:00:00〜11:59:59 : 0XD8〜0XDF
												// 4日目12:00:00〜14:59:59 : 0XE0〜0XE7
												// 4日目15:00:00〜17:59:59 : 0XE8〜0XEF
												// 4日目18:00:00〜20:59:59 : 0XF0〜0XF7
												// 4日目21:00:00〜23:59:59 : 0XF8〜0XFF
												// ここまで48 (6byte)

		uint8_t		lastSectionNumber:8;		// 8bit  サブテーブルの最後のセクション(すなわち、最大のセクション番号を持つセクション)の番号
		uint16_t	transportStreamId:16;		// 16bit トランスポート識別
		uint16_t	originalNetworkId:16;		// 16bit オリジナルネットワーク識別
		uint8_t		segmentLastSectionNumber:8;	// 8bit  present/following の場合、last_section_numberと同一の0x01固定。
												//		 schedule の場合、当該segment毎に、その中で使用するセクションの最終section_numberを記述する。
		uint8_t		lastTableId:8;				// 8bit  最終の table_id を記述 present/following の場合、table_id と同一。
												//		 schedule の場合、最終 table_id を入れる
												// ここまで112 (14byte)

		uint8_t		*sectionData;				// 複数のパケットに存在するセクションデータを蓄積するエリア 4096-13-4(CRC)=4079byte以下となる
		uint8_t		CRC32[4];
} EIT;



// SdtDescriptor
typedef struct {
		uint16_t	serviceId:16; 				// 16bit サービス識別
		uint8_t		reservedFutureUse:3; 		// 3bit  未定義
		uint8_t		EitUserDefinedFlags:3;		// 3bit  EIT事業者定義フラグ
		uint8_t		EitScheduleFlag:1;  		// 1bit  EIT[対モジュール]フラグ 0:なし 1:あり
		uint8_t		EitPresentFollowingFlag:1;	// 1bit  EIT[現在 /次]フラグ 0:なし 1:あり
		uint8_t		runningStatus:3;			// 3bit  進行状態
		uint8_t		freeCaMode:1;				// 1bit 0: 無料番組 1: 有料番組
		uint16_t	descriptorsLoopLength:12;	// 12bit 後続記述子の全byte数
												//			例： セクション長245の場合セクション実サイズは233である
												//               -12: -8 11-3, -4: CRC32 4byte
												//				 そこからserviceId〜descriptorsLoopLength まで5byteを
												//				 差し引いた228(byte)が最大値となる
												// ここまで40bit(5byte)
		uint8_t		*descriptor;				// variable size 記述子領域
												// uint8_t[descriptorsLoopLength]
} SdtDescriptor;

// EitDescriptor
typedef struct {
		uint16_t	eventId:16; 				// 16bit イベントID
		uint64_t	startTime:40;				// 40bit following の場合のみ all bit 1 のとき未定義と判断する
		uint32_t	duration:24;				// 24bit present/following の場合のみ all bit 1 のとき未定義と判断する
												//       MAX48(時間)
		uint8_t		runningStatus:3;			// 3bit  0:当該イベントが有効
												//		 0以外:0とみなして処理する
		uint8_t		freeCaMode:1;				// 1bit 0: 無料番組 1: 有料番組
		uint16_t	descriptorsLoopLength:12;	// 12bit 後続記述子の全byte数
												// ここまで96bit(12byte)
		uint8_t		*descriptor;				// variable size 記述子領域
} EitDescriptor;



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

// DescriptorX4D 短形式イベント記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0x4D
		uint8_t		descriptorLength:8;					// 短形式イベント記述子の記述子長
		uint32_t	ISO639LanguageCode:24;				// 言語コード“jpn(”0x6A706E”)”を記述
		uint8_t		eventNameLength:8;					// 番組名長として 80byte(全角 40 文字)以下の値を記述
		uint8_t		*eventNameChar;						// 番組名を80byte(全角40文字)以下で記述する。改行コードは使用しない。
														// 同一ループ内にシリーズ記述子が存在し、かつその中にシリーズ名が記載されていない場合は、
														// この番組名がシリーズ名を兼ねる。
		uint8_t		textLength:8;						// 番組記述長を 160byte 以下の値で記述
		uint8_t		*textChar;							// 番組記述を 160byte(全角 80 文字)以下で記述


} DescriptorX4D;

// DescriptorX50 コンポーネント記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0x50
		uint8_t		descriptorLength:8;					// コンポーネント記述子の記述子長
		uint8_t		reservedFutureUse:4;				//
		uint8_t		streamContent:4;					// 0x01(映像)を記述
		uint8_t		componentType:8;					// 0X01 : 映像 480i(525i)、アスペクト比 4:3
														// 0X03 : 映像 480i(525i)、アスペクト比 16:9 パンベクトルなし
														// 0X04 : 映像 480i(525i)、アスペクト比>16:9
														// 0XA1 : 映像 480p(525p)、アスペクト比 4:3
														// 0XA3 : 映像 480p(525p)、アスペクト比 16:9 パンベクトルなし
														// 0XA4 : 映像 480p(525p)、アスペクト比>16:9
														// 0XB1 : 映像 1080i(1125i)、アスペクト比 4:3
														// 0XB3 : 映像 1080i(1125i)、アスペクト比 16:9 パンベクトルなし
														// 0XB4 : 映像 1080i(1125i)、アスペクト比>16:9
														// 0XC1 : 映像 720p(750p)、アスペクト比 4:3
														// 0XC3 : 映像 720p(750p)、アスペクト比 16:9
														// 0XC4 : 映像 720p(750p)、アスペクト比>16:9
														// 注: BS デジタル放送ではパンベクトルは運用しない
		uint8_t		componentTag:8;						// 当該番組内で一意となるコンポーネントタグ値で、
														// PMT のストリーム識別子のコンポーネントタグ値と対応させて利用できる。
		uint32_t	ISO639LanguageCode:24;				// 言語コード“jpn(”0x6A706E”)”を記述
		uint8_t		*textChar;							// 複数映像コンポーネント存在時に映像種類名として16byte(全角8文字)以下で記述する。
														// 改行コードは使用しない。コンポーネント記述がデフォルトの文字列である場合は
														// このフィールドを省略することができる。
} DescriptorX50;

// DescriptorXC4 音声コンポーネント記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xC4
		uint8_t		descriptorLength:8;					// 音声コンポーネント記述子の記述子長
		uint8_t		reservedFutureUse:4;				//
		uint8_t		streamContent:4;					// 0x02(音声)を記述
		uint8_t		componentType:8;					// 0X01 : 1/0 モード(シングルモノ)
														// 0X02 : 1/0+1/0 モード(デュアルモノ)
														// 0X03 : 2/0 モード(ステレオ)
														// 0X07 : 3/1 モード
														// 0X08 : 3/2 モード
														// 0X09 : 3/2+LFE モード
		uint8_t		componentTag:8;						// 
		uint8_t		streamType:8;						// 0x0Fを記述
		uint8_t		simulcastGroupTag:8;				// サイマルキャストグループ識別を記述する。
														// サイマルキャストを行っているコンポーネントには同一番号を割り当てる
		uint8_t		ESMultiLingualFlag:1;				// ES 多言語フラグを記述する。デュアルモノで2か国語多重の場合1を記述
		uint8_t		mainComponentFlag:1;				// 主コンポーネントフラグを記述する。その音声コンポーネントが主音声であるとき1を記述する
		uint8_t		qualityIndicator:2;					// 音質表示を記述
		uint8_t		samplingRate:3;						// 当該音声コンポーネントのサンプリング周波数を記述
														// 101 : 32kHz
														// 111 : 48kHz
		uint8_t		reserved:1;							//
		uint32_t	ISO639LanguageCode:24;				// 音声コンポーネントの言語名を記述
		uint32_t	ISO639LanguageCode2:24;				// ES多言語モードにおいて、第2音声コンポーネントの言語名を記述
														// 0x6A706E : jpn 日本語
														// 0x656E67 : eng 英語
														// 0x646575 : deu ドイツ語
														// 0x667261 : fra フランス語
														// 0x697461 : ita イタリア語
														// 0x727573 : rus ロシア語
														// 0x7A686F : zho 中国語
														// 0x6B6F72 : kor 韓国語
														// 0x737061 : spa スペイン語
														// 0x657463 : etc 外国語 上記以外の言語
		uint8_t		*textChar;							// 音声種類名として16byteまたは全角8文字以下で記述する。
														// 1ESによるデュアルモノラルの場合は、各音声種類名の間に改行コード1byteを入れ、
														// 合計33byte(全角16文字)以下で記述する

} DescriptorXC4;

// DescriptorXC7 データコンテンツ記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xC4
		uint8_t		descriptorLength:8;					// データコンテンツ記述子の記述子長
		uint16_t	dataComponentId:16;					// データ符号化方式識別
		uint8_t		entryComponent:8;					// エントリコンポーネント
		uint8_t		selectorLength:8;					// セレクタ長 後続のセレクタ領域のバイト長を規定
		uint8_t		*selectorByte;						// 
		uint8_t		numOfComponentRef:8;					// 参照コンポーネント数 この記述子の表すコンテンツの記録再生に必要な、
														// イベント内の全コンポーネントストリーム(ただしエントリコンポーネントで
														// 指定されたコンポーネントストリームを除く)の個数を表す。
														// この個数は後続の参照コンポーネントのループのバイト長と一致する。
		uint8_t		*componentRef;						// 当該コンテンツの視聴もしくは記録に必要なイベント内のコンポーネントストリーム
														// (ただしエントリコンポーネントで指定されたコンポーネントストリームを除く)
														// のコンポーネントタグを記述
		uint32_t	ISO639LanguageCode:24;				// 後続のサービス記述で使用される文字記述の言語を ISO 639-2(27)に規定される
														// アルファベット 3 文字コードで表す
		uint8_t		textLength:8;						// コンテンツ記述長 後続のコンテンツ記述のバイト長
		uint8_t		*textChar;							// 伝送されるコンテンツに関する説明を記述
} DescriptorXC7;

// DescriptorX54 コンテント記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0x54
		uint8_t		descriptorLength:8;					// コンテント記述子の記述子長
														// ループ回数の最大値を 7(content_nibble 指定:3、user_nibble 指定:4)と規定する。
														// すなわち記述子長の最大値は 14byte とする。
														// 14byte を超えた部分の記述は無視しても良い。
/*
	struct nibble {
			uint8_t	contentNibbleLebel1:4;				// 番組ジャンル大分類を記述する。番組特性を示す際には”0xE”を指定する。
														// 0xEのときは、ジャンルとして判断しない(後続のuser_nibbleで何らかの番組特性が指定されていると判断する)
			uint8_t	contentNibbleLebel2:4;				// 番組ジャンル中分類を記述する。content_nibble_level1=0xEのときは、番組特性コード表の種類を記述する
			uint8_t	userNibble1:4;						// content_nibble_level1=0xEのときのみ、番組特性として判断する。
														// content_nibble=0xE0のときは、BS デジタル放送用番組付属情報と判断する
														// content_nibble_level1!=0xEのときは、いかなる値が入っていても無視する
														// 将来ダウンロード等により、content_nibble_level1=0xE(番組特性指示)に対する
														// content_nibble_level2(番組特性コード表種類)が追加された場合は、
														// その追加された番組特性コード表に従って判断する

			uint8_t	userNibble2:4;
	} **nibbleArray;									// ポインタ配列(NULLストップ) 最大7 content_nibble 指定:3、user_nibble 指定:4
*/
		uint8_t *nibble;
} DescriptorX54;


// DescriptorXC1 デジタルコピー制御記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xC1
		uint8_t		descriptorLength:8;					// デジタルコピー制御記述子の記述子長
		uint8_t		digitalRecordingControlData:2;		// 
		uint8_t		maximumBitRateFlag:1;				// 0 : 当該サービスの最大伝送レートを記述しない場合
														// 1 : 当該サービスの最大伝送レートを記述する場合
		uint8_t		componentControlFlag:1;				// 0 : デジタルコピー制御情報は番組全体について規定され、
														//     コンポーネント制御長以降のフィールドは存在しない
														// 1 : コンポーネント制御長以降のフィールドが有効となり、
														//     デジタルコピー制御情報は番組を構成するコンポーネント毎に規定
		uint8_t		copyControlType:2;					// コピー世代を制御する形式の情報
		uint8_t		APSControlData:2;					// アナログ出力コピー制御情報である
														// copy_control_type が01および11の場合のアナログ出力のコピーを制御する情報を表す
/*
		uint8_t		MaximumBitRate:8;					// 最大伝送レートを記述
		uint8_t		componentControlLength:8;			// 
	struct componentControl {
			uint8_t		componentTag:8;					// コンポーネント毎に規定する場合、配置可能なコンポーネントは、component_tagの値が
														//  0x40〜0x7F をとるコンポーネントのみとする
			uint8_t		digitalRecordingControlData:2;	//
			uint8_t		maximumBitRateFlag:1;			// 
			uint8_t		reservedFutureUse:1;			// 
			uint8_t		copyControlType:2;				// 
			uint8_t		APSControlData:2;				// 
			uint8_t		MaximumBitRate:8;				// 
	} **componentArray;									// ポインタ配列(NULLストップ)
*/
		uint8_t *controlData;
														// 詳細は4-TR-B15v6_9-2p4 (BS／広帯域CSデジタル放送運用規定).pdf 4-278 参照
} DescriptorXC1;


// DescriptorX55 パレンタルレート記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xC1
		uint8_t		descriptorLength:8;					// デジタルコピー制御記述子の記述子長
														// 1loopのみ
														// 2loop目以降が存在する場合、無効 JPN以外は無効
		uint32_t	countryCode:24;						// JPN(0x4A504E)を記述
		uint8_t		rating:8;							// 視聴者の推奨最低年齢を記述
														// 0X00       : 未定義(指定なし)
														// 0X01〜0X11 : 最小年齢=rating+3歳
														// 0X12〜0XFF : 事業者定義
} DescriptorX55;


// DescriptorXD6 イベントグループ記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xD6
		uint8_t		descriptorLength:8;					// イベントグループ記述子の記述子長
		uint8_t		groupType:4;						// イベントのグループ種別
														// 0x1 : イベント共有
														// 0x2 : イベントリレー
														// 0x3 : イベント移動
		uint8_t		eventCount:4;						// 後続のeventIdループ数
		uint8_t		*eventGroup;
/*
	struct event {
			uint16_t	serviceId:16;					// 関連付けるサービス識別
			uint16_t	eventId:16;						// 関連付けるイベント識別
	} **eventArray;										// ポインタ配列(NULLストップ) eventCount+1
		uint8_t		*privateDataByte;					// 未定義 データが入っていても無視する
*/
} DescriptorXD6;


// DescriptorXD9 コンポーネントグループ記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xD9
		uint8_t		descriptorLength:8;					// コンポーネントグループ記述子の記述子長
		uint8_t		componentGroupType:3;				// コンポーネントグループの種別
														// 000:マルチビューTV サービス
														// 001-111:将来のため予約
		uint8_t		totalBitRateFlag:1;					// イベント中のコンポーネントグループ内の総ビットレートの記述状態を示す
														// 0:コンポーネントグループ内の総ビットレートフィールドが当該記述子中に存在しない
														// 1:コンポーネントグループ内の総ビットレートフィールドが当該記述子中に存在する
		uint8_t		numOfGroup:4;						// イベント内でのコンポーネントグループの数
/*
	struct group {
			uint8_t		componentGroupId:4;				// コンポーネントグループ識別を記述
														// 0x0:メイングループ
														// 0x1-0xF:サブグループ
			uint8_t		numOfCAUnit:4;					// コンポーネントグループ内での課金/非課金単位の数
		struct	CAUnit {
				uint8_t		CAUnitId:4;					// コンポーネントが属する課金単位識別
														// 0x0:非課金単位グループ
														// 0x1:デフォルト ES 群を含む課金単位グループ
				uint8_t		numOfComponent:4;			// 当該コンポーネントグループに属し、かつ直前の CA_unit_id で示される
														// 課金/非課金単位に属するコンポーネントの数
				uint8_t		*componentTag;				// 8bit  componentTag[numOfComponent]
														// コンポーネントグループに属するコンポーネントタグ値
		} **caArray;									// *caArray[numOfCAUnit]
			uint8_t		totalBitRate:8;					// コンポーネントグループ内のコンポーネントの総ビットレートを示す
			uint8_t		textLength:8;					// 後続のコンポーネントグループ記述のバイト長
			uint8_t		*textChar;						// 8bit  textChar[textLength]
														// コンポーネントグループに関する説明を記述
	} **groupArray;										// *groupArray[numOfGroup]
*/
		uint8_t		*componentGroup;
} DescriptorXD9;


// DescriptorXD5 シリーズ記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xD5
		uint8_t		descriptorLength:8;					// シリーズ記述子の記述子長
		uint16_t	seriesId:16;						// シリーズをユニークに識別するための識別子
		uint8_t		repeatLabel:4;						// 再放送の回数。シリーズの放送期間とシリーズの再放送の放送期間が重なる場合に、
														// 再放送の番組の repeat_label を 0x1 とする。
														// 更にもう一つ再放送がある場合、repeat_label を 0x2 とする。
														// 同期間に本放送と、更に最大 15 編成で同一シリーズを再放送することができる。
														// 本放送には 0x0 を与える。
														// 数字には具体的な意味はなく、編成を区別するラベルとしての意味だけである
		uint8_t		programPattern:3;					// シリーズ番組の編成のパターンを表す
														// 0x0 : 不定期(0x1-0x7 で定義されるもの以外)
														// 0x1 : 帯番組(毎日、平日のみ毎日、土・日のみなど)、週に複数回の編成
														// 0x2 : 週に 1 回の編成(毎週火曜日など)
														// 0x3 : 月に 1 回の編成
														// 0x4 : 同日内に複数話数編成
														// 0x5 : 長時間番組の分割
														// 0x6-0x7 : reserved
		uint8_t		expireDateValidFlag:1;				// 次に続く expire_date の値が有効であることを示すフラグ。
														// シリーズの終了予定日の値が有効な場合、この値を 1 とする
		uint16_t	expireDate:16;						// シリーズが有効な期限を示す。年月日をMJDで表す。
														// 何らかの原因で最終回のイベントを認識できなかった場合も、
														// この日付を過ぎると受信機はシリーズが終了したと認識する
		uint16_t	episodeNumber:12;					// この記述子が示す番組の、シリーズ内の話数を示す。
														// 第1回から第4095回まで記載できる。
														// 話数がこれを超える場合はシリーズを別に定義する。
														// 0x000 連続番組の場合で番組回数が定義できない場合に付与する。
		uint16_t	lastEpisodeNumber:12;				// 当該シリーズ番組の番組総数を示す。
														// 第1回から第4095回まで記載できる。
														// 番組総数がこれを超える場合はシリーズを別に定義する。
														// 0x000 最終回が未定の場合に付与する
		uint8_t		*seriesNameChar;					// シリーズ名を表す文字コード
} DescriptorXD5;


// DescriptorXDC LDTリンク記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xDC
		uint8_t		descriptorLength:8;					// LDTリンク記述子の記述子長
		uint16_t	originalServiceId:16;				// リンクするLDTサブテーブルのオリジナルサービス識別
		uint16_t	transportStreamId:16;				// リンクするLDTサブテーブルが含まれるトランスポートストリーム識別
		uint16_t	originalNetworkId:16;				// リンクするLDTサブテーブルが含まれる元の分配システムのネットワーク識別
		uint8_t		*description;
} DescriptorXDC;


// DescriptorX4E 拡張形式イベント記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0x4E
		uint8_t		descriptorLength:8;					// 拡張形式イベント記述子長
		uint8_t		descriptorNumber:4;					// 情報を分割して記述する際の拡張形式イベント記述子番号を記述する。
														// ・項目名ごとに記述する場合。
														// ・項目名で 200byte を超える記述を行う場合。
														// この時、次のフィールドは初期化せずに送る必要がある。
		uint8_t		lastDescriptorNumber:4;				// 関係づけられた記述子の最終拡張形式イベント記述子番号を記述
		uint32_t	ISO639LanguageCode:24;				// jpn(0x6A706E)を記述
		uint8_t		lengthOfItems:8;					// 項目長を記述
		uint8_t		itemDescriptionLength:8;			// 項目名長を16byte(全角8文字)以下の値で記述
		uint8_t		*itemDescriptionChar;				// 16byte(全角8文字)以下で項目名を記述
		uint8_t		itemLength:8;						// 項目記述長を200byte(全角100文字)以下の値で記述
		uint8_t		*itemChar;							// 200byte(全角100文字)以下で項目記述を記述
		uint8_t		textLength:8;						// 0x00とする
		uint8_t		*textChar;							// 記述しない
} DescriptorX4E;


// DescriptorX42 スタッフ記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0x42
		uint8_t		descriptorLength:8;					// スタッフ記述子長
		uint8_t		*stuffingByte;						// all1とする いかなる値が入っていても無視する
} DescriptorX42;


// DescriptorXC5 ハイパーリンク記述子
typedef struct {
		uint8_t		descriptorTag:8;					// uimsbf 8bit 0xc5
		uint8_t		descriptorLength:8;					// ハイパーリンク記述子長
		uint8_t		hyperLinkageType:8;					// ハイパーリンク種別
		uint8_t		linkDestinationType:8;				// リンク先種別
		uint8_t		selectorLength:8;					// セレクタ長
		uint8_t		*selector;							// 後続 (セレクタ領域 + プライベート領域)
} DescriptorXC5;


// 文字列が数値で指定した桁数であることをチェックする
static bool isdigit_n(int mode, char *num, char *out, size_t size)
{
	bool rtn = true;

	if(mode==10){
		if(strlen(num) == size){
			for(int i=0; i<size;i++){
				if(!isdigit(*(num+i))){
					rtn = false;
					break;
				}
			}
		}else{
			rtn = false;
		}
	}else if(mode==16){
		if(!strncmp(num, "0x", 2) || !strncmp(num, "0X", 2) ){
			num+=2;
		}
		if(strlen(num) == size){
			for(int i=0; i<size;i++){
				if(!isxdigit(*(num+i))){
					rtn = false;
					break;
				}
			}
		}else{
			rtn = false;
		}
	}else{
		rtn = false;
	}

	if(rtn){
		strcpy(out, num);
	}
	return(rtn);
}

// 実行時オプション解析
bool parseOption(int argc, char *argv[], ARG_PARAM *param)
{

	struct option long_options[] = {
		{"help",		no_argument,		NULL,	'h'},
		{"pid",			required_argument,	NULL,	'p'},
		{"sid",			required_argument,	NULL,	's'},
		{"file",		required_argument,	NULL,	'f'},
		{NULL,			0,					NULL,	0}
	};

	bool rtn = true;
	int c;
	char buf[16];

	//memset(param, '\0', sizeof(ARG_PARAM));
	while(true){
		if ((c = getopt_long(argc, argv, "hp:s:f:", long_options,
			NULL)) == -1) {
			break;
		}

		switch(c){
		case 'h':
			fprintf(stderr, "usege %s --pid 999 --sid 999 --file file path\n", argv[0]);
			return(false);
			break;
		case 'p':
			fprintf(stderr, "--pid option %s\n", (optarg==NULL)?"NULL":optarg);
			if(optarg){
				if(isdigit_n(16, optarg, buf, 3)){
					param->pid = strtol(buf,NULL,10);
				}else{
					fprintf(stderr, "--pid arg error %s\n", optarg);
					rtn = false;
				}
			}
			break;
		case 's':
			fprintf(stderr, "--sid option %s\n", (optarg==NULL)?"NULL":optarg);
			if(optarg){
				if(isdigit_n(10, optarg, buf, 3)){
					param->sid = strtol(buf,NULL,10);
				}else{
					fprintf(stderr, "--sid arg error %s\n", optarg);
					rtn = false;
				}
			}
			break;
		case 'f':
			if(optarg){
				if(strncmp(optarg, "-", 1) && strncmp(optarg, "--", 2)){
					param->file = strdup(optarg);
				}else{
					fprintf(stderr, "--file arg error %s  --file filename\n", optarg);
					rtn = false;
				}
			}else{
				fprintf(stderr, "--file arg null\n" );
				rtn = false;
			}
			break;
		default:
			fprintf(stderr, "Error: Unknown character code %c\n", c);
			rtn = false;
			break;
		}

		if(!rtn){
			break;
		}
	}

	if(!rtn){
		return(false);
	}

	if(optind==1){
		fprintf(stderr, "usege %s --pid 999 --sid 999 --file file path\n", argv[0]);
		return(false);
	}

	if(param->file==NULL){
		fprintf(stderr, "Please specify the ts file\n" );
		rtn = false;
	}

	return(rtn);
}

// 修正ユリウス日から西暦計算
static void calc_mjd(int *year, int *month, int *day, int *week, int mjd)
{
	int jdd,jds;

	// 修正ユリウス日を西暦1年1月1日からの日数に変換
	jdd = mjd + 678576;
	*week = (mjd+3) % 7;
	// 日数は1から始まるので0から始まるように1を引き西暦1年3月1日からの日数にする
	jdd		= jdd-1-31-28+365;
	jds		= jdd%146097%36524%1461%365+jdd%146097/36524/4*365+jdd%146097%36524%1461/365/4*365;
	*year	= jdd/146097*400+jdd%146097/36524*100-jdd%146097/36524/4+jdd%146097%36524/1461*4+jdd%146097%36524%1461/365-jdd%146097%36524%1461/365/4;
	*month	= jds/153*5+jds%153/61*2+jds%153%61/31+3;
	*day	= jds%153%61%31+1;
	if(*month>12) {
		(*year)+=1;
		(*month)-=12;
	}
	(*month)--;
}

static void calc_bcd(int *out, uint8_t bcd)
{
	*out = (bcd>>4 & 0x0f) * 10 + (bcd & 0x0f);
} 

static void dateTime(struct tm *tp, uint64_t startTime)
{
	uint16_t mjd = (startTime>>32 & 0xff) << 8 | (startTime>>24 & 0xff) ;
	calc_mjd(&tp->tm_year, &tp->tm_mon, &tp->tm_mday, &tp->tm_wday, mjd);
	calc_bcd(&tp->tm_hour, startTime>>16 & 0xff);
	calc_bcd(&tp->tm_min, startTime>>8 & 0xff);
	calc_bcd(&tp->tm_sec, startTime & 0xff);
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

/***
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
***/

static void
EIT_set(uint8_t *payload, EIT *eit)
{
	eit->tableId = *payload;
	eit->sectionSyntaxIndicator = *(payload+1)>>7 & 0x01;
	eit->reservedFutureUse1		= *(payload+1)>>6 & 0x01;
	eit->reserved1				= *(payload+1)>>4 & 0x03;
	eit->sectionLength			= (*(payload+1)&0x0f)<<8 | *(payload+2);

	eit->serviceId		= (*(payload+3)&0xff)<<8 | *(payload+4);

	eit->reserved2				= *(payload+5)>>6 & 0x03;
	eit->versionNumber			= *(payload+5)>>1 & 0x1f;
	eit->currentNextIndicator	= *(payload+5)>>1 & 0x01;
	eit->sectionNumber			= *(payload+6);
	eit->lastSectionNumber		= *(payload+7);

	eit->transportStreamId		= (*(payload+8)&0xff)<<8 | *(payload+9);
	eit->originalNetworkId		= (*(payload+10)&0xff)<<8 | *(payload+11);

	eit->segmentLastSectionNumber	= *(payload+12);
	eit->lastTableId			= *(payload+13);
	eit->sectionData			= payload+14;
	memcpy(eit->CRC32, payload+3+eit->sectionLength-4, 4);

	return;
}

/***
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
***/

static void
EitDescriptor_set(uint8_t *sectionData, EitDescriptor *edesc)
{
	edesc->eventId					= (*sectionData&0xff)<<8 | *(sectionData+1);
	edesc->startTime				= (uint64_t)*(sectionData+2)<<32 | (uint64_t)*(sectionData+3)<<24 | (uint64_t)*(sectionData+4)<<16 | (uint64_t)*(sectionData+5)<<8 | (uint64_t)*(sectionData+6);
	edesc->duration					= *(sectionData+7)<<16 | *(sectionData+8)<<8 | *(sectionData+9);
	edesc->runningStatus			= *(sectionData+10)>>5 & 0x07;
	edesc->freeCaMode				= *(sectionData+10)>>4 & 0x01;
	edesc->descriptorsLoopLength	= (*(sectionData+10)&0x0f)<<8 | *(sectionData+11);
	edesc->descriptor				= sectionData+12;

	return;
}

/***
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
***/

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

static void
DescriptorX4D_set(uint8_t *descriptor, DescriptorX4D *x4D)
{
	x4D->descriptorTag					= *descriptor;
	x4D->descriptorLength				= *(descriptor+1);
	x4D->ISO639LanguageCode				= *(descriptor+2) << 16 | *(descriptor+3) << 8 | *(descriptor+4);
	x4D->eventNameLength				= *(descriptor+5);
	x4D->eventNameChar					= descriptor+6;
	x4D->textLength						= *(descriptor+6+x4D->eventNameLength);
	x4D->textChar						= descriptor+6+x4D->eventNameLength+1;

	return;
}

static void
DescriptorX4E_set(uint8_t *descriptor, DescriptorX4E *x4E)
{
	x4E->descriptorTag					= *descriptor;
	x4E->descriptorLength				= *(descriptor+1);
	x4E->descriptorNumber				= *(descriptor+2)>>4&0x0f;
	x4E->lastDescriptorNumber			= *(descriptor+2)&0x0f;
	x4E->ISO639LanguageCode				= *(descriptor+3) << 16 | *(descriptor+4) << 8 | *(descriptor+5);
	x4E->lengthOfItems					= *(descriptor+6);
	x4E->itemDescriptionLength			= *(descriptor+7);
	x4E->itemDescriptionChar			= (descriptor+8);
	x4E->itemLength						= *(descriptor+8+x4E->itemDescriptionLength);
	x4E->itemChar						= descriptor+8+x4E->itemDescriptionLength+1;
	x4E->textLength						= *(descriptor+8+x4E->itemDescriptionLength+1+x4E->itemLength);
	x4E->textChar						= NULL;
	return;
}

static void
DescriptorX50_set(uint8_t *descriptor, DescriptorX50 *x50)
{
	x50->descriptorTag					= *descriptor;
	x50->descriptorLength				= *(descriptor+1);
	x50->reservedFutureUse				= *(descriptor+2)>>4&0x0f;
	x50->streamContent					= *(descriptor+2)&0x0f;
	x50->componentType					= *(descriptor+3);
	x50->componentTag					= *(descriptor+4);
	x50->ISO639LanguageCode				= *(descriptor+5) << 16 | *(descriptor+6) << 8 | *(descriptor+7);
	x50->textChar						= descriptor+8;

	return;
}

static void
DescriptorXC4_set(uint8_t *descriptor, DescriptorXC4 *xC4)
{
	xC4->descriptorTag					= *descriptor;
	xC4->descriptorLength				= *(descriptor+1);
	xC4->reservedFutureUse				= *(descriptor+2)>>4&0x0f;
	xC4->streamContent					= *(descriptor+2)&0x0f;
	xC4->componentType					= *(descriptor+3);
	xC4->componentTag					= *(descriptor+4);
	xC4->streamType						= *(descriptor+5);
	xC4->simulcastGroupTag				= *(descriptor+6);
	xC4->ESMultiLingualFlag				= *(descriptor+7)>>7&0x01;
	xC4->mainComponentFlag				= *(descriptor+7)>>6&0x01;
	xC4->qualityIndicator				= *(descriptor+7)>>5&0x03;
	xC4->samplingRate					= *(descriptor+7)>>1&0x07;
	xC4->reserved						= *(descriptor+7)&0x01;
	xC4->ISO639LanguageCode				= *(descriptor+8) << 16 | *(descriptor+9) << 8 | *(descriptor+10);
	if(xC4->ESMultiLingualFlag==1){
		xC4->ISO639LanguageCode2			= *(descriptor+11) << 16 | *(descriptor+12) << 8 | *(descriptor+13);
		xC4->textChar						= descriptor+14;
	}else{
		xC4->textChar						= descriptor+11;
	}

	return;
}

static void
DescriptorXC7_set(uint8_t *descriptor, DescriptorXC7 *xC7)
{
	xC7->descriptorTag					= *descriptor;
	xC7->descriptorLength				= *(descriptor+1);
	xC7->dataComponentId				= *(descriptor+2) << 8 | *(descriptor+3);
	xC7->entryComponent					= *(descriptor+4);
	xC7->selectorLength					= *(descriptor+5);
	xC7->selectorByte					= descriptor+6;
	xC7->numOfComponentRef				= *(descriptor+6+xC7->selectorLength);
	xC7->componentRef					= descriptor+6+xC7->selectorLength+1;
	xC7->ISO639LanguageCode				=	*(descriptor+6+xC7->selectorLength+1+xC7->numOfComponentRef) << 16 |
											*(descriptor+6+xC7->selectorLength+1+xC7->numOfComponentRef+1) << 8 |
											*(descriptor+6+xC7->selectorLength+1+xC7->numOfComponentRef+2);
	xC7->textLength						= *(descriptor+6+xC7->selectorLength+1+xC7->numOfComponentRef+3);
	xC7->textChar						= descriptor+6+xC7->selectorLength+1+xC7->numOfComponentRef+3+1;

	return;
}

static void
DescriptorX54_set(uint8_t *descriptor, DescriptorX54 *x54)
{
	x54->descriptorTag					= *descriptor;
	x54->descriptorLength				= *(descriptor+1);
	x54->nibble							= descriptor+2;

	return;
}

static void
DescriptorXC1_set(uint8_t *descriptor, DescriptorXC1 *xC1)
{
	xC1->descriptorTag					= *descriptor;
	xC1->descriptorLength				= *(descriptor+1);
	xC1->digitalRecordingControlData	= *(descriptor+2) >> 6 & 0x03;
	xC1->maximumBitRateFlag				= *(descriptor+2) >> 5 & 0x01;
	xC1->componentControlFlag			= *(descriptor+2) >> 4 & 0x01;
	xC1->copyControlType				= *(descriptor+2) >> 2 & 0x03;
	xC1->APSControlData					= *(descriptor+2) & 0x03;

	xC1->controlData					= descriptor+3;

	return;
}

static void
DescriptorXD6_set(uint8_t *descriptor, DescriptorXD6 *xD6)
{
	xD6->descriptorTag					= *descriptor;
	xD6->descriptorLength				= *(descriptor+1);
	xD6->groupType						= *(descriptor+2) >> 4 & 0x0f;
	xD6->eventCount						= *(descriptor+2) & 0x0f;
	xD6->eventGroup						= descriptor+3;

	return;
}


static void
DescriptorX55_set(uint8_t *descriptor, DescriptorX55 *x55)
{
	x55->descriptorTag					= *descriptor;
	x55->descriptorLength				= *(descriptor+1);
	x55->countryCode					= *(descriptor+2) << 16 | *(descriptor+3) << 8 | *(descriptor+4);
	x55->rating							= *(descriptor+5);

	return;
}


static void
DescriptorXD9_set(uint8_t *descriptor, DescriptorXD9 *xD9)
{
	xD9->descriptorTag					= *descriptor;
	xD9->descriptorLength				= *(descriptor+1);
	xD9->componentGroupType				= *(descriptor+2) >> 5 & 0x07;
	xD9->totalBitRateFlag				= *(descriptor+2) >> 4 & 0x01;
	xD9->numOfGroup						= *(descriptor+2) & 0x0f;
	xD9->componentGroup					= descriptor+3;

	return;
}

static void
DescriptorXDC_set(uint8_t *descriptor, DescriptorXDC *xDC)
{
	xDC->descriptorTag					= *descriptor;
	xDC->descriptorLength				= *(descriptor+1);
	xDC->originalServiceId				= *(descriptor+2) << 8 | *(descriptor+3);
	xDC->transportStreamId				=*(descriptor+4) << 8 | *(descriptor+5);
	xDC->originalNetworkId				= *(descriptor+6) << 8 | *(descriptor+7);
	xDC->description					= descriptor+8;

	return;
}

static void
DescriptorXD5_set(uint8_t *descriptor, DescriptorXD5 *xD5)
{
	xD5->descriptorTag					= *descriptor;
	xD5->descriptorLength				= *(descriptor+1);
	xD5->seriesId						= *(descriptor+2) << 8 | *(descriptor+3);
	xD5->repeatLabel					=*(descriptor+4) >> 4 & 0x0f;
	xD5->programPattern					= *(descriptor+4) >> 1 & 0x07;
	xD5->expireDateValidFlag			= *(descriptor+4) & 0x01;
	xD5->expireDate						= *(descriptor+5) << 8 | *(descriptor+6);
	xD5->episodeNumber					= *(descriptor+7) << 4 | *(descriptor+8) >> 4 | 0x0f;
	xD5->lastEpisodeNumber				= (*(descriptor+8) & 0x0f) << 8 | *(descriptor+9);
	xD5->seriesNameChar					= descriptor+10;

	return;
}

static void
DescriptorX42_set(uint8_t *descriptor, DescriptorX42 *x42)
{
	x42->descriptorTag					= *descriptor;
	x42->descriptorLength				= *(descriptor+1);
	x42->stuffingByte					= descriptor+2;

	return;
}

static void
DescriptorXC5_set(uint8_t *descriptor, DescriptorXC5 *xC5)
{
	xC5->descriptorTag					= *descriptor;
	xC5->descriptorLength				= *(descriptor+1);
	xC5->hyperLinkageType				= *(descriptor+2);
	xC5->linkDestinationType			= *(descriptor+3);
	xC5->selectorLength					= *(descriptor+4);
	xC5->selector						= descriptor+5;

	return;
}


static void printEIT(EIT *eit)
{
	fprintf(stdout, "------------------------------------------------------\n");
	fprintf(stdout, "[EIT TABLE]\n");
	fprintf(stdout, "tableId                  : %02" PRIx8" ",  eit->tableId);
	switch(eit->tableId){
	case 0x4E : fprintf(stdout," [自ストリームの現在と次の番組]\n");break;
	case 0x4F : fprintf(stdout," [他ストリームの現在と次の番組]\n");break;
	case 0x50 : fprintf(stdout," [自ストリームの本日〜 4日の番組]\n");break;
	case 0x51 : fprintf(stdout," [自ストリームの 5日〜 8日の番組]\n");break;
	case 0x52 : fprintf(stdout," [自ストリームの 9日〜12日の番組]\n");break;
	case 0x53 : fprintf(stdout," [自ストリームの13日〜16日の番組]\n");break;
	case 0x54 : fprintf(stdout," [自ストリームの17日〜20日の番組]\n");break;
	case 0x55 : fprintf(stdout," [自ストリームの21日〜24日の番組]\n");break;
	case 0x56 : fprintf(stdout," [自ストリームの25日〜28日の番組]\n");break;
	case 0x57 : fprintf(stdout," [自ストリームの29日〜32日の番組]\n");break;
	case 0x60 : fprintf(stdout," [他ストリームの本日〜 4日の番組]\n");break;
	case 0x61 : fprintf(stdout," [他ストリームの 5日〜 8日の番組]\n");break;
	case 0x62 : fprintf(stdout," [他ストリームの 9日〜12日の番組]\n");break;
	case 0x63 : fprintf(stdout," [他ストリームの13日〜16日の番組]\n");break;
	case 0x64 : fprintf(stdout," [他ストリームの17日〜20日の番組]\n");break;
	case 0x65 : fprintf(stdout," [他ストリームの21日〜24日の番組]\n");break;
	case 0x66 : fprintf(stdout," [他ストリームの25日〜28日の番組]\n");break;
	case 0x67 : fprintf(stdout," [他ストリームの29日〜32日の番組]\n");break;
	default:    fprintf(stdout," [tableId unknown]\n");
	}
	fprintf(stdout, "sectionSyntaxIndicator   : %" PRIx8"\n",    eit->sectionSyntaxIndicator);
	fprintf(stdout, "reservedFutureUse1       : %" PRIx8"\n",    eit->reservedFutureUse1);
	fprintf(stdout, "reserved1                : %02" PRIx8"\n",  eit->reserved1);
	// EIT MAX size 4096byte なのでsectionLengthの上限値は4093となる
	fprintf(stdout, "sectionLength            : %04" PRIx16 " [%" PRId16 "]\n", eit->sectionLength, eit->sectionLength);
	fprintf(stdout, "serviceId                : %04" PRIx16 " [%" PRId16 "]\n", eit->serviceId, eit->serviceId);
	fprintf(stdout, "reserved2                : %02" PRIx8"\n",  eit->reserved2);
	fprintf(stdout, "versionNumber            : %02" PRIx8"\n",  eit->versionNumber);
	fprintf(stdout, "currentNextIndicator     : %" PRIx8"\n",    eit->currentNextIndicator);
	fprintf(stdout, "sectionNumber            : %02" PRIx8"",  eit->sectionNumber);
	if(eit->sectionNumber >= 0x00 && eit->sectionNumber < 0x08){
		fprintf(stdout, " [1日目 0:00:00〜 2:59:59 : 0X00〜0X07]\n");
	}else if(eit->sectionNumber >= 0x08 && eit->sectionNumber < 0x10){
		fprintf(stdout, " [1日目 3:00:00〜 5:59:59 : 0X08〜0X0F]\n");
	}else if(eit->sectionNumber >= 0x10 && eit->sectionNumber < 0x18){
		fprintf(stdout, " [1日目 6:00:00〜 8:59:59 : 0X10〜0X17]\n");
	}else if(eit->sectionNumber >= 0x18 && eit->sectionNumber < 0x20){
		fprintf(stdout, " [1日目 9:00:00〜11:59:59 : 0X18〜0X1F]\n");
	}else if(eit->sectionNumber >= 0x20 && eit->sectionNumber < 0x28){
		fprintf(stdout, " [1日目12:00:00〜14:59:59 : 0X20〜0X27]\n");
	}else if(eit->sectionNumber >= 0x28 && eit->sectionNumber < 0x30){
		fprintf(stdout, " [1日目15:00:00〜17:59:59 : 0X28〜0X2F]\n");
	}else if(eit->sectionNumber >= 0x30 && eit->sectionNumber < 0x38){
		fprintf(stdout, " [1日目18:00:00〜20:59:59 : 0X30〜0X37]\n");
	}else if(eit->sectionNumber >= 0x38 && eit->sectionNumber < 0x40){
		fprintf(stdout, " [1日目21:00:00〜23:59:59 : 0X38〜0X3F]\n");
	}else if(eit->sectionNumber >= 0x40 && eit->sectionNumber < 0x48){
		fprintf(stdout, " [2日目 0:00:00〜 2:59:59 : 0X40〜0X47]\n");
	}else if(eit->sectionNumber >= 0x48 && eit->sectionNumber < 0x50){
		fprintf(stdout, " [2日目 3:00:00〜 5:59:59 : 0X48〜0X4F]\n");
	}else if(eit->sectionNumber >= 0x50 && eit->sectionNumber < 0x58){
		fprintf(stdout, " [2日目 6:00:00〜 8:59:59 : 0X50〜0X57]\n");
	}else if(eit->sectionNumber >= 0x58 && eit->sectionNumber < 0x60){
		fprintf(stdout, " [2日目 9:00:00〜11:59:59 : 0X58〜0X5F]\n");
	}else if(eit->sectionNumber >= 0x60 && eit->sectionNumber < 0x68){
		fprintf(stdout, " [2日目12:00:00〜14:59:59 : 0X60〜0X67]\n");
	}else if(eit->sectionNumber >= 0x68 && eit->sectionNumber < 0x70){
		fprintf(stdout, " [2日目15:00:00〜17:59:59 : 0X68〜0X6F]\n");
	}else if(eit->sectionNumber >= 0x70 && eit->sectionNumber < 0x78){
		fprintf(stdout, " [2日目18:00:00〜20:59:59 : 0X70〜0X77]\n");
	}else if(eit->sectionNumber >= 0x78 && eit->sectionNumber < 0x80){
		fprintf(stdout, " [2日目21:00:00〜23:59:59 : 0X78〜0X7F]\n");
	}else if(eit->sectionNumber >= 0x80 && eit->sectionNumber < 0x88){
		fprintf(stdout, " [3日目 0:00:00〜 2:59:59 : 0X80〜0X87]\n");
	}else if(eit->sectionNumber >= 0x88 && eit->sectionNumber < 0x90){
		fprintf(stdout, " [3日目 3:00:00〜 5:59:59 : 0X88〜0X8F]\n");
	}else if(eit->sectionNumber >= 0x90 && eit->sectionNumber < 0x98){
		fprintf(stdout, " [3日目 6:00:00〜 8:59:59 : 0X90〜0X97]\n");
	}else if(eit->sectionNumber >= 0x98 && eit->sectionNumber < 0xa0){
		fprintf(stdout, " [3日目 9:00:00〜11:59:59 : 0X98〜0X9F]\n");
	}else if(eit->sectionNumber >= 0xa0 && eit->sectionNumber < 0xa8){
		fprintf(stdout, " [3日目12:00:00〜14:59:59 : 0XA0〜0XA7]\n");
	}else if(eit->sectionNumber >= 0xa8 && eit->sectionNumber < 0xb0){
		fprintf(stdout, " [3日目15:00:00〜17:59:59 : 0XA8〜0XAF]\n");
	}else if(eit->sectionNumber >= 0xb0 && eit->sectionNumber < 0xb8){
		fprintf(stdout, " [3日目18:00:00〜20:59:59 : 0XB0〜0XB7]\n");
	}else if(eit->sectionNumber >= 0xb8 && eit->sectionNumber < 0xc0){
		fprintf(stdout, " [3日目21:00:00〜23:59:59 : 0XB8〜0XBF]\n");
	}else if(eit->sectionNumber >= 0xc0 && eit->sectionNumber < 0xc8){
		fprintf(stdout, " [4日目 0:00:00〜 2:59:59 : 0XC0〜0XC7]\n");
	}else if(eit->sectionNumber >= 0xc8 && eit->sectionNumber < 0xd0){
		fprintf(stdout, " [4日目 3:00:00〜 5:59:59 : 0XC8〜0XCF]\n");
	}else if(eit->sectionNumber >= 0xd0 && eit->sectionNumber < 0xd8){
		fprintf(stdout, " [4日目 6:00:00〜 8:59:59 : 0XD0〜0XD7]\n");
	}else if(eit->sectionNumber >= 0xd8 && eit->sectionNumber < 0xe0){
		fprintf(stdout, " [4日目 9:00:00〜11:59:59 : 0XD8〜0XDF]\n");
	}else if(eit->sectionNumber >= 0xe0 && eit->sectionNumber < 0xe8){
		fprintf(stdout, " [4日目12:00:00〜14:59:59 : 0XE0〜0XE7]\n");
	}else if(eit->sectionNumber >= 0xe8 && eit->sectionNumber < 0xf0){
		fprintf(stdout, " [4日目15:00:00〜17:59:59 : 0XE8〜0XEF]\n");
	}else if(eit->sectionNumber >= 0xf0 && eit->sectionNumber < 0xf8){
		fprintf(stdout, " [4日目18:00:00〜20:59:59 : 0XF0〜0XF7]\n");
	}else if(eit->sectionNumber >= 0xf8 && eit->sectionNumber < 0x100){
		fprintf(stdout, " [4日目21:00:00〜23:59:59 : 0XF8〜0XFF]\n");
	}else{
		fprintf(stdout, " [un known]\n");
	}
	fprintf(stdout, "lastSectionNumber        : %02" PRIx8"\n",  eit->lastSectionNumber);
	fprintf(stdout, "transportStreamId        : %04" PRIx16"\n", eit->transportStreamId);
	fprintf(stdout, "originalNetworkId        : %04" PRIx16"\n", eit->originalNetworkId);
	fprintf(stdout, "segmentLastSectionNumber : %02" PRIx8"\n",  eit->segmentLastSectionNumber);
	fprintf(stdout, "sectionData              : [size %" PRId16 "]", eit->sectionLength-11);
	hex_dump(eit->sectionData,eit->sectionLength-11,0);
	fprintf(stdout, "CRC32                  : ");
	hex_dump(eit->CRC32,4,0);

	return;
}

static void printEitDescriptor(EitDescriptor *edesc)
{
	struct tm t;
	fprintf(stdout, "\t------------------------------------------------------\n");
	fprintf(stdout, "\t[EVENT INFOMATION]\n");
	fprintf(stdout, "\teventId               : %04" PRIx16 " [%" PRId16 "]\n", edesc->eventId, edesc->eventId);
	dateTime(&t, edesc->startTime);
	fprintf(stdout, "\tstartTime             : %010" PRIx64 " [mjd %" PRId16 " %04" PRId32 "/%02" PRId32 "/%02" PRId32 " %02" PRId32 ":%02" PRId32 ":%02" PRId32 " [%" PRId32 "]""\n",
		(uint64_t)edesc->startTime, (uint16_t)((edesc->startTime>>32 & 0xff) << 8 | (edesc->startTime>>24 & 0xff )), t.tm_year,t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, t.tm_wday);
	fprintf(stdout, "\tduration              : %08" PRIx32"\n",  edesc->duration);
	fprintf(stdout, "\trunningStatus         : %02" PRIx8"\n",  edesc->runningStatus);
	fprintf(stdout, "\tfreeCaMode            : %" PRIx8"\n",    edesc->freeCaMode);
	fprintf(stdout, "\tdescriptorsLoopLength : %04" PRIx16 " [%" PRId16 "]\n", edesc->descriptorsLoopLength, edesc->descriptorsLoopLength);
	fprintf(stdout, "\tdescriptor              : \n");

	return;
}

/***
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
***/

static void printDescriptorXCB(DescriptorXCB *xCB)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xCB->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xCB->descriptorLength, xCB->descriptorLength);
	fprintf(stdout, "\t\tCaSystemId                     : %04" PRIx16"\n", xCB->CaSystemId);
	fprintf(stdout, "\t\tCaUnitId                       : %02" PRIx8"\n",  xCB->CaUnitId);
	fprintf(stdout, "\t\tnumOfComponent                 : %02" PRIx8"\n",  xCB->numOfComponent);
	fprintf(stdout, "\t\tcomponentTag                   : %02" PRIx8"\n",  xCB->componentTag);
	fprintf(stdout, "\t\tcontractVerificationInfoLength : %02" PRIx8 " [%" PRId8 "]\n",  xCB->contractVerificationInfoLength, xCB->contractVerificationInfoLength);
	fprintf(stdout, "\t\tcontractVerificationInfo       : ");
	hex_dump(xCB->contractVerificationInfo,xCB->contractVerificationInfoLength,2);
	fprintf(stdout, "\t\tfeeNameLength                  : %02" PRIx8"\n",  xCB->feeNameLength);
	fprintf(stdout, "\t\tfeeName                        : ");
	(xCB->feeNameLength>0) ? (hex_dump(xCB->feeName,xCB->feeNameLength,2)) : fprintf(stdout, "\n");

	return;
}

static void printDescriptorX4D(DescriptorX4D *x4D)
{
	const char *option = "-S -w";
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  x4D->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", x4D->descriptorLength, x4D->descriptorLength);
	fprintf(stdout, "\t\tISO639LanguageCode jpn:0x6A706E: %06" PRIx32 "\n", x4D->ISO639LanguageCode);
	fprintf(stdout, "\t\teventNameLength                : %02" PRIx8 " [%" PRId8 "]\n", x4D->eventNameLength, x4D->eventNameLength);
	fprintf(stdout, "\t\teventNameChar                  : "); 
	if(x4D->eventNameLength>0){
		hex_dump(x4D->eventNameChar,x4D->eventNameLength,2);
   		 uint8_t *sjis = aribTOsjis(x4D->eventNameChar, x4D->eventNameLength);
   		 if(sjis!=NULL){
			uint8_t  *p;
   		 	if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
   		 }
	}else{
		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\t\ttextLength                     : %02" PRIx8 " [%" PRId8 "]\n", x4D->textLength, x4D->textLength);
	fprintf(stdout, "\t\ttextChar                       : "); 
	if(x4D->textLength>0){
		hex_dump(x4D->textChar,x4D->textLength,2);
		uint8_t *sjis = aribTOsjis(x4D->textChar, x4D->textLength);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
	}else{
		fprintf(stdout, "\n");
	}

	return;
}

static void printDescriptorX4E(DescriptorX4E *x4E)
{
	const char *option = "-S -w";
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  x4E->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", x4E->descriptorLength, x4E->descriptorLength);
	fprintf(stdout, "\t\tdescriptorNumber               : %02" PRIx8"\n",  x4E->descriptorNumber);
	fprintf(stdout, "\t\tlastDescriptorNumber           : %02" PRIx8"\n",  x4E->lastDescriptorNumber);
	fprintf(stdout, "\t\tISO639LanguageCode jpn:0x6A706E: %06" PRIx32 "\n", x4E->ISO639LanguageCode);
	fprintf(stdout, "\t\tlengthOfItems                  : %02" PRIx8 " [%" PRId8 "]\n", x4E->lengthOfItems, x4E->lengthOfItems);
	fprintf(stdout, "\t\titemDescriptionLength          : %02" PRIx8 " [%" PRId8 "]\n", x4E->itemDescriptionLength, x4E->itemDescriptionLength);
	fprintf(stdout, "\t\titemDescriptionChar            : "); 

	if(x4E->itemDescriptionLength>0){
		hex_dump(x4E->itemDescriptionChar,x4E->itemDescriptionLength,2);
		uint8_t *sjis = aribTOsjis(x4E->itemDescriptionChar, x4E->itemDescriptionLength);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
	}else{
		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\t\titemLength                     : %02" PRIx8 " [%" PRId8 "]\n", x4E->itemLength, x4E->itemLength);
	fprintf(stdout, "\t\titemChar                       : "); 
	if(x4E->itemLength>0){
		hex_dump(x4E->itemChar,x4E->itemLength,2);
		uint8_t *sjis = aribTOsjis(x4E->itemChar, x4E->itemLength);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
	}else{
		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\t\ttextLength                     : %02" PRIx8"\n",  x4E->textLength);
	fprintf(stdout, "\t\ttextChar                       : "); 
	if(x4E->textLength>0){
		hex_dump(x4E->textChar,x4E->textLength,2);
		uint8_t *sjis = aribTOsjis(x4E->textChar, x4E->textLength);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
	}else{
		fprintf(stdout, "\n");
	}

}

static void printDescriptorX50(DescriptorX50 *x50)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  x50->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", x50->descriptorLength, x50->descriptorLength);
	fprintf(stdout, "\t\treservedFutureUse              : %02" PRIx8"\n",  x50->reservedFutureUse);
	fprintf(stdout, "\t\tstreamContent 0x01(映像)       : %02" PRIx8"\n",  x50->streamContent);
	fprintf(stdout, "\t\tcomponentType                  : %02" PRIx8"", x50->componentType);
	switch(x50->componentType){
	case 0X01 : fprintf(stdout, " [映像 480i(525i)、アスペクト比 4:3]\n"); break;
	case 0X03 : fprintf(stdout, " [映像 480i(525i)、アスペクト比 16:9 パンベクトルなし]\n"); break;
	case 0X04 : fprintf(stdout, " [映像 480i(525i)、アスペクト比>16:9]\n"); break;
	case 0XA1 : fprintf(stdout, " [映像 480p(525p)、アスペクト比 4:3]\n"); break;
	case 0XA3 : fprintf(stdout, " [映像 480p(525p)、アスペクト比 16:9 パンベクトルなし]\n"); break;
	case 0XA4 : fprintf(stdout, " [映像 480p(525p)、アスペクト比>16:9]\n"); break;
	case 0XB1 : fprintf(stdout, " [映像 1080i(1125i)、アスペクト比 4:3]\n"); break;
	case 0XB3 : fprintf(stdout, " [映像 1080i(1125i)、アスペクト比 16:9 パンベクトルなし]\n"); break;
	case 0XB4 : fprintf(stdout, " [映像 1080i(1125i)、アスペクト比>16:9]\n"); break;
	case 0XC1 : fprintf(stdout, " [映像 720p(750p)、アスペクト比 4:3]\n"); break;
	case 0XC3 : fprintf(stdout, " [映像 720p(750p)、アスペクト比 16:9]\n"); break;
	case 0XC4 : fprintf(stdout, " [映像 720p(750p)、アスペクト比>16:9]\n"); break;
	default: fprintf(stdout, " [unknown ???]\n"); break;
	}
	fprintf(stdout, "\t\tcomponentTag                   : %02" PRIx8"\n",  x50->componentTag);
	fprintf(stdout, "\t\tISO639LanguageCode             : %06" PRIx32 "", x50->ISO639LanguageCode);
	switch(x50->ISO639LanguageCode){
	case 0x6A706E : fprintf(stdout, " [jpn 日本語]\n"); break;
	case 0x656E67 : fprintf(stdout, " [eng 英語]\n"); break;
	case 0x646575 : fprintf(stdout, " [deu ドイツ語]\n"); break;
	case 0x667261 : fprintf(stdout, " [fra フランス語]\n"); break;
	case 0x697461 : fprintf(stdout, " [ita イタリア語]\n"); break;
	case 0x727573 : fprintf(stdout, " [rus ロシア語]\n"); break;
	case 0x7A686F : fprintf(stdout, " [zho 中国語]\n"); break;
	case 0x6B6F72 : fprintf(stdout, " [kor 韓国語]\n"); break;
	case 0x737061 : fprintf(stdout, " [spa スペイン語]\n"); break;
	case 0x657463 : fprintf(stdout, " [etc 外国語 上記以外の言語]\n"); break;
	default: fprintf(stdout, " [unknown ???]\n"); break;
	}
	fprintf(stdout, "\t\ttextChar                       : "); 
	uint8_t txtLength = x50->descriptorLength-6;
	if(txtLength>0){
		hex_dump(x50->textChar,txtLength, 2);
		const char *option = "-S -w";
    	uint8_t *sjis = aribTOsjis(x50->textChar, txtLength);
    	if(sjis!=NULL){
			uint8_t  *p;
    		if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
    	}
	}else{
		fprintf(stdout, "\n");
	}

	return;
}

static void printDescriptorXC4(DescriptorXC4 *xC4)
{
	uint8_t txtLength;
	const char *option = "-S -w";

	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xC4->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xC4->descriptorLength, xC4->descriptorLength);
	fprintf(stdout, "\t\treservedFutureUse              : %02" PRIx8"\n",  xC4->reservedFutureUse);
	fprintf(stdout, "\t\tstreamContent 0x02(音声)       : %02" PRIx8"\n",  xC4->streamContent);
	fprintf(stdout, "\t\tcomponentType                  : %02" PRIx8"", xC4->componentType);
	switch(xC4->componentType){
	case 0X01 : fprintf(stdout, " [1/0 モード(シングルモノ)]\n"); break;
	case 0X02 : fprintf(stdout, " [1/0+1/0 モード(デュアルモノ)]\n"); break;
	case 0X03 : fprintf(stdout, " [2/0 モード(ステレオ)]\n"); break;
	case 0X07 : fprintf(stdout, " [3/1 モード]\n"); break;
	case 0X08 : fprintf(stdout, " [3/2 モード]\n"); break;
	case 0X09 : fprintf(stdout, " [3/2+LFE モード]\n"); break;
	default: fprintf(stdout, " [unknown ???]\n"); break;
	}
	fprintf(stdout, "\t\tcomponentTag                   : %02" PRIx8"\n",  xC4->componentTag);
	fprintf(stdout, "\t\tstreamType (0x0F)              : %02" PRIx8"\n",  xC4->streamType);
	fprintf(stdout, "\t\tsimulcastGroupTag              : %02" PRIx8"\n",  xC4->simulcastGroupTag);
	fprintf(stdout, "\t\tESMultiLingualFlag             : %02" PRIx8"\n",  xC4->ESMultiLingualFlag);
	fprintf(stdout, "\t\tmainComponentFlag              : %02" PRIx8"\n",  xC4->mainComponentFlag);
	fprintf(stdout, "\t\tqualityIndicator               : %02" PRIx8"\n",  xC4->qualityIndicator);
	fprintf(stdout, "\t\tsamplingRate                   : %02" PRIx8"",  xC4->samplingRate);
	switch(xC4->samplingRate){
	case 5 : fprintf(stdout," [101:32kHz]\n"); break;
	case 7 : fprintf(stdout," [111:48kHz]\n"); break;
	default: fprintf(stdout, " [unknown ???]\n"); break;
	}
	fprintf(stdout, "\t\treserved                       : %02" PRIx8"\n",  xC4->reserved);

	fprintf(stdout, "\t\tISO639LanguageCode             : %06" PRIx32 "", xC4->ISO639LanguageCode);
	switch(xC4->ISO639LanguageCode){
	case 0x6A706E : fprintf(stdout, " [jpn 日本語]\n"); break;
	case 0x656E67 : fprintf(stdout, " [eng 英語]\n"); break;
	case 0x646575 : fprintf(stdout, " [deu ドイツ語]\n"); break;
	case 0x667261 : fprintf(stdout, " [fra フランス語]\n"); break;
	case 0x697461 : fprintf(stdout, " [ita イタリア語]\n"); break;
	case 0x727573 : fprintf(stdout, " [rus ロシア語]\n"); break;
	case 0x7A686F : fprintf(stdout, " [zho 中国語]\n"); break;
	case 0x6B6F72 : fprintf(stdout, " [kor 韓国語]\n"); break;
	case 0x737061 : fprintf(stdout, " [spa スペイン語]\n"); break;
	case 0x657463 : fprintf(stdout, " [etc 外国語 上記以外の言語]\n"); break;
	default: fprintf(stdout, " [unknown ???]\n"); break;
	}

	if(xC4->ESMultiLingualFlag==1){
		fprintf(stdout, "\t\tISO639LanguageCode2            : %06" PRIx32 "", xC4->ISO639LanguageCode2);
		switch(xC4->ISO639LanguageCode2){
		case 0x6A706E : fprintf(stdout, " [jpn 日本語]\n"); break;
		case 0x656E67 : fprintf(stdout, " [eng 英語]\n"); break;
		case 0x646575 : fprintf(stdout, " [deu ドイツ語]\n"); break;
		case 0x667261 : fprintf(stdout, " [fra フランス語]\n"); break;
		case 0x697461 : fprintf(stdout, " [ita イタリア語]\n"); break;
		case 0x727573 : fprintf(stdout, " [rus ロシア語]\n"); break;
		case 0x7A686F : fprintf(stdout, " [zho 中国語]\n"); break;
		case 0x6B6F72 : fprintf(stdout, " [kor 韓国語]\n"); break;
		case 0x737061 : fprintf(stdout, " [spa スペイン語]\n"); break;
		case 0x657463 : fprintf(stdout, " [etc 外国語 上記以外の言語]\n"); break;
		default: fprintf(stdout, " [unknown ???]\n"); break;
		}
		txtLength = xC4->descriptorLength-12;
	}else{
		txtLength = xC4->descriptorLength-9;
	}

	fprintf(stdout, "\t\ttextChar                       : "); 
	if(txtLength>0){
		hex_dump(xC4->textChar,txtLength, 2);
		uint8_t *sjis = aribTOsjis(xC4->textChar, txtLength);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
	}else{
		fprintf(stdout, "\n");
	}

	return;
}


static void printDescriptorXC7(DescriptorXC7 *xC7)
{
	const char *option = "-S -w";

	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xC7->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xC7->descriptorLength, xC7->descriptorLength);
	fprintf(stdout, "\t\tdataComponentId                : %04" PRIx16"\n",  xC7->dataComponentId);
	fprintf(stdout, "\t\tentryComponent                 : %02" PRIx8"\n",  xC7->entryComponent);
	fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC7->selectorLength, xC7->selectorLength);
	fprintf(stdout, "\t\tselectorByte                   : ");
	if(xC7->selectorLength>0){
		hex_dump(xC7->selectorByte, xC7->selectorLength, 2);
	}else{
		fprintf(stdout, "\n");
	}
	fprintf(stdout, "\t\tnumOfComponentRef              : %02" PRIx8 " [%" PRId8 "]\n", xC7->numOfComponentRef, xC7->numOfComponentRef);
	fprintf(stdout, "\t\tcomponentRef                   : ");
	if(xC7->numOfComponentRef>0){
		hex_dump(xC7->componentRef, xC7->numOfComponentRef, 2);
	}else{
		fprintf(stdout, "\n");
	}
	fprintf(stdout, "\t\tISO639LanguageCode             : %06" PRIx32 "", xC7->ISO639LanguageCode);
	switch(xC7->ISO639LanguageCode){
	case 0x6A706E : fprintf(stdout, " [jpn 日本語]\n"); break;
	case 0x656E67 : fprintf(stdout, " [eng 英語]\n"); break;
	case 0x646575 : fprintf(stdout, " [deu ドイツ語]\n"); break;
	case 0x667261 : fprintf(stdout, " [fra フランス語]\n"); break;
	case 0x697461 : fprintf(stdout, " [ita イタリア語]\n"); break;
	case 0x727573 : fprintf(stdout, " [rus ロシア語]\n"); break;
	case 0x7A686F : fprintf(stdout, " [zho 中国語]\n"); break;
	case 0x6B6F72 : fprintf(stdout, " [kor 韓国語]\n"); break;
	case 0x737061 : fprintf(stdout, " [spa スペイン語]\n"); break;
	case 0x657463 : fprintf(stdout, " [etc 外国語 上記以外の言語]\n"); break;
	default: fprintf(stdout, " [unknown ???]\n"); break;
	}
	fprintf(stdout, "\t\ttextLength                     : %02" PRIx8 " [%" PRId8 "]\n", xC7->textLength, xC7->textLength);
	fprintf(stdout, "\t\ttextChar                       : ");
	if(xC7->textLength>0){
		hex_dump(xC7->textChar, xC7->textLength, 2);
		uint8_t *sjis = aribTOsjis(xC7->textChar, xC7->textLength);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
	}else{
		fprintf(stdout, "\n");
	}

	return;
}

static void printDescriptorX54(DescriptorX54 *x54)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  x54->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", x54->descriptorLength, x54->descriptorLength);
	fprintf(stdout, "\t\tnibble                         : ");
	if(x54->descriptorLength>0){
		hex_dump(x54->nibble, x54->descriptorLength, 2);
	}else{
		fprintf(stdout, "\n");
	}

	return;
}

static void printDescriptorXC1(DescriptorXC1 *xC1)
{
/*
	uint8_t componentControlLength = 0;
	uint8_t *cur;
*/
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xC1->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xC1->descriptorLength, xC1->descriptorLength);
	fprintf(stdout, "\t\tdigitalRecordingControlData    : %02" PRIx8 "\n", xC1->digitalRecordingControlData);
	fprintf(stdout, "\t\tmaximumBitRateFlag             : %02" PRIx8 "\n", xC1->maximumBitRateFlag);
	fprintf(stdout, "\t\tcomponentControlFlag           : %02" PRIx8 "\n", xC1->componentControlFlag);
	fprintf(stdout, "\t\tcopyControlType                : %02" PRIx8 "\n", xC1->copyControlType);
	fprintf(stdout, "\t\t%s: %02" PRIx8 "\n", (xC1->copyControlType==0x01 || xC1->copyControlType==0x03) ? "APSControlData                 " :"reservedFutureUse              ", xC1->APSControlData);
	fprintf(stdout, "\t\tcontrolData                    : ");
	if(xC1->descriptorLength-1>0){
		hex_dump(xC1->controlData, xC1->descriptorLength-1, 2);
	}else{
		fprintf(stdout, "\n");
	}
	if(xC1->maximumBitRateFlag==0x01){
		fprintf(stdout, "\t\tmaximumBitRate                 : %02" PRIx8 "\n", *(xC1->controlData));
	}

/*
	if(xC1->maximumBitRateFlag==0x01){
		fprintf(stdout, "\t\tmaximumBitRate                 : %02" PRIx8 "\n", *xC1->controlData);
		cur = xC1->controlData+1;
	}else{
		cur = xC1->controlData;
	}
*/

	return;
}

static void printDescriptorXD6(DescriptorXD6 *xD6)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xD6->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xD6->descriptorLength, xD6->descriptorLength);
	fprintf(stdout, "\t\tgroupType                      : %02" PRIx8"",  xD6->groupType);
	switch(xD6->groupType){
	case 0x01 : fprintf(stdout, " [イベント共有]\n"); break;
	case 0x02 : fprintf(stdout, " [イベントリレー]\n"); break;
	case 0x03 : fprintf(stdout, " [イベント移動]\n"); break;
	default : fprintf(stdout, " [un known ???]\n"); break;
	}
	fprintf(stdout, "\t\teventCount                     : %02" PRIx8 " [%" PRId8 "]\n", xD6->eventCount, xD6->eventCount);
	fprintf(stdout, "\t\teventGroup                     : ");
	if(xD6->descriptorLength-1>0){
		hex_dump(xD6->eventGroup, xD6->descriptorLength-1, 2);
	}else{
		fprintf(stdout, "\n");
	}

	for(int i=0; i<xD6->eventCount; i++){
		uint16_t serviceId,eventId;
		serviceId = *(xD6->eventGroup+(i*4))<<8 | *(xD6->eventGroup+(i*4+1));
		eventId = *(xD6->eventGroup+(i*4+2))<<8 | *(xD6->eventGroup+(i*4+3));
		fprintf(stdout, "\t\teventGroup[%d] serviceId %04" PRIx16 "[%" PRId16 "] eventId %04" PRIx16 "[%" PRId16 "]\n", i,
			serviceId,serviceId,eventId,eventId);
	}

	return;
}

static void printDescriptorX55(DescriptorX55 *x55)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  x55->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", x55->descriptorLength, x55->descriptorLength);
	fprintf(stdout, "\t\tcountryCode JPN(0x4a504e)      : %06" PRIx32 "\n", x55->countryCode);
	fprintf(stdout, "\t\trating                         : %02" PRIx8 "", x55->rating);
	if(x55->rating==0){
		fprintf(stdout, "[未定義(指定なし)]\n");
	}else if(x55->rating>=0x01 || x55->rating <= 0x11){
		fprintf(stdout, "[最小年齢:%d歳]\n", x55->rating+3);
	}else{
		fprintf(stdout, "[事業者定義]\n");
	}

	return;
}


static void printDescriptorXD9(DescriptorXD9 *xD9)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xD9->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xD9->descriptorLength, xD9->descriptorLength);
	fprintf(stdout, "\t\tcomponentGroupType             : %02" PRIx8"",  xD9->componentGroupType);
	fprintf(stdout, " [%s]\n", (xD9->componentGroupType==0x00) ? "マルチビューTV" : "将来のため予約");
	fprintf(stdout, "\t\ttotalBitRateFlag               : %02" PRIx8"\n",  xD9->totalBitRateFlag);
	fprintf(stdout, "\t\tnumOfGroup                     : %02" PRIx8 " [%" PRId8 "]\n", xD9->numOfGroup, xD9->numOfGroup);

	uint8_t *cur = xD9->componentGroup;
	if(xD9->descriptorLength-1>0){
		hex_dump(cur, xD9->descriptorLength-1, 2);
		for(int i=0; i<xD9->numOfGroup; i++){
			uint8_t componentGroupId,numOfCaUnit;
			componentGroupId = *cur>>4|0x0f;
			numOfCaUnit = *cur|0x0f;
			fprintf(stdout, "\t\tcomponentGroup[%d] componentGroupId %02" PRIx8 " numOfCaUnit %02" PRIx8" [%" PRId8 "]\n",
				i, componentGroupId, numOfCaUnit, numOfCaUnit);
			cur++;
			for(int k=0; i<numOfCaUnit; k++){
				uint8_t CaUnitId,numOfComponent;
				CaUnitId = *cur>>4|0x0f;
				numOfComponent = *cur|0x0f;
				fprintf(stdout, "\t\t\tcomponent[%d] CaUnitId %02" PRIx8 " numOfComponent %02" PRIx8" [%" PRId8 "]\n",
					k, CaUnitId, numOfComponent, numOfComponent);
				cur++;
				for(int n=0; n<numOfComponent; n++){
					fprintf(stdout, "\t\t\t\t[%d] componentTag[%02" PRIx8 "]\n", n, *cur);
					cur++;
				}
			}

			if(xD9->totalBitRateFlag==0x01){
				fprintf(stdout, "\t\ttotalBitRate                   : %02" PRIx8"\n",  *cur);
				cur++;
			}

			uint8_t textLength;
			textLength = *cur;
			cur++;
			fprintf(stdout, "\t\ttextLength                     : %02" PRIx8 " [%" PRId8 "]\n", textLength, textLength);
			fprintf(stdout, "\t\ttext                           : ");
			(textLength>0) ? (hex_dump(cur, textLength, 3)) : fprintf(stdout, "\n");;
		}
	}else{
		fprintf(stdout, "\n");
	}

	return;
}

static void printDescriptorXDC(DescriptorXDC *xDC)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xDC->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xDC->descriptorLength, xDC->descriptorLength);
	fprintf(stdout, "\t\toriginalServiceId              : %04" PRIx16" [%" PRId16 "]\n",  xDC->originalServiceId, xDC->originalServiceId);
	fprintf(stdout, "\t\ttransportStreamId              : %04" PRIx16" [%" PRId16 "]\n",  xDC->transportStreamId, xDC->transportStreamId);
	fprintf(stdout, "\t\toriginalNetworkId              : %04" PRIx16"\n",  xDC->originalNetworkId);

	for(int i=0; i<xDC->descriptorLength-6; i+=4){
		uint8_t *cur = xDC->description+i;
		uint16_t descriptionId = *cur << 8 | *(cur+1);
		uint8_t  reservedFutureUse = *(cur+2)>>4 && 0x0f;
		uint8_t  descriptionType = *(cur+2) && 0x0f;
		uint8_t  userDefined = *(cur+3);
		fprintf(stdout, "\t\tdescriptionId                  : %04" PRIx16"\n",  descriptionId);
		fprintf(stdout, "\t\treservedFutureUse              : %02" PRIx8"\n",  reservedFutureUse);
		fprintf(stdout, "\t\tdescriptionType                : %02" PRIx8"",  descriptionType);
		switch(descriptionType){
		case 0x00 : fprintf(stdout, " [未定義]\n"); break;
		case 0x01 : fprintf(stdout, " [短形式イベント記述子]\n"); break;
		case 0x02 : fprintf(stdout, " [拡張形式イベント記述子(項目名を記述しない独立形式)]\n"); break;
		case 0x03 : fprintf(stdout, " [拡張形式イベント記述子]\n"); break;
		default:
			if(descriptionType>=0x04 && descriptionType<=0x0e){
				fprintf(stdout, " [将来の使用のためリザーブ]\n");
			}else{
				fprintf(stdout, " [その他(記述形式を特定しない、混在する、を含む)]\n");
			}
			break;
		}
		fprintf(stdout, "\t\tuserDefined                    : %02" PRIx8"\n",  userDefined);
	}

	return;
}

static void printDescriptorXD5(DescriptorXD5 *xD5)
{
	const char *option = "-S -w";

	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xD5->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xD5->descriptorLength, xD5->descriptorLength);
	fprintf(stdout, "\t\tseriesId                       : %04" PRIx16" \n",  xD5->seriesId);
	fprintf(stdout, "\t\trepeatLabel                    : %02" PRIx8"\n",  xD5->repeatLabel);
	fprintf(stdout, "\t\tprogramPattern                 : %02" PRIx8"\n",  xD5->programPattern);
	fprintf(stdout, "\t\texpireDateValidFlag            : %02" PRIx8"\n",  xD5->expireDateValidFlag);
	fprintf(stdout, "\t\texpireDate                     : %04" PRIx16"\n",  xD5->expireDate);
	fprintf(stdout, "\t\tepisodeNumber                  : %04" PRIx16"\n",  xD5->episodeNumber);
	fprintf(stdout, "\t\tlastEpisodeNumber              : %04" PRIx16"\n",  xD5->lastEpisodeNumber);
	fprintf(stdout, "\t\tseriesNameChar                 : " );
	if(xD5->descriptorLength-8>0){
		hex_dump(xD5->seriesNameChar, xD5->descriptorLength-8, 2);
		uint8_t *sjis = aribTOsjis(xD5->seriesNameChar, xD5->descriptorLength-8);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
	}else{
		fprintf(stdout, "\n");
	}

	return;
}

static void printDescriptorX42(DescriptorX42 *x42)
{
	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  x42->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", x42->descriptorLength, x42->descriptorLength);
	fprintf(stdout, "\t\tstuffingByte                   : ");
	if(x42->descriptorLength>0){
		hex_dump(x42->stuffingByte, x42->descriptorLength, 2);
	}else{
		fprintf(stdout, "\n");
	}

	return;
}

// DescriptorXC5 ハイパーリンク記述子
static void printDescriptorXC5(DescriptorXC5 *xC5)
{
	const char *option = "-S -w";

	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  xC5->descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", xC5->descriptorLength, xC5->descriptorLength);
	fprintf(stdout, "\t\thyperLinkageType               : %02" PRIx8"",  xC5->hyperLinkageType);
	switch(xC5->hyperLinkageType){
	case 0x00 : fprintf(stdout, " [reserved]\n"); break;
	case 0x01 : fprintf(stdout, " [combined data]\n"); break;
	case 0x02 : fprintf(stdout, " [combined stream]\n"); break;
	case 0x03 : fprintf(stdout, " [content to index]\n"); break;
	case 0x04 : fprintf(stdout, " [index to content]\n"); break;
	case 0x05 : fprintf(stdout, " [guide data]\n"); break;
	case 0x06 : fprintf(stdout, " [未定義]\n"); break;
	case 0x07 : fprintf(stdout, " [content to metadata]\n"); break;
	case 0x08 : fprintf(stdout, " [metadata to content]\n"); break;
	case 0x09 : fprintf(stdout, " [portal URI]\n"); break;
	case 0x0a : fprintf(stdout, " [authority URI]\n"); break;
	default:
		if(xC5->hyperLinkageType>=0x0b && xC5->hyperLinkageType <= 0x3f){
			fprintf(stdout, " [未定義]\n");
		}else if(xC5->hyperLinkageType==0x40){
			fprintf(stdout, " [index module]\n");
		}else if(xC5->hyperLinkageType>=0x41 && xC5->hyperLinkageType <= 0x7f){
			fprintf(stdout, " [未定義]\n");
		}else{ // 0x80 <= xC5->hyperLinkageType <= 0xff
			fprintf(stdout, " [ユーザー定義のリンク種別]\n");
		}
	break;
	}

	fprintf(stdout, "\t\tlinkDestinationType            : %02" PRIx8"",  xC5->linkDestinationType);
	switch(xC5->linkDestinationType){
	case 0x00 :
		fprintf(stdout, " [reserved]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
	case 0x01 :
		fprintf(stdout, " [link to service]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
		if(xC5->selectorLength==6){
			uint16_t originalNetworkId	= *(xC5->selector) << 8 | *(xC5->selector+1);
			uint16_t transportStreamId	= *(xC5->selector+2) << 8 | *(xC5->selector+3);
			uint16_t serviceId			= *(xC5->selector+4) << 8 | *(xC5->selector+5);
			fprintf(stdout, "\t\t\toriginalNetworkId : %04" PRIx16 "\n", originalNetworkId);
			fprintf(stdout, "\t\t\ttransportStreamId : %04" PRIx16 "\n", transportStreamId);
			fprintf(stdout, "\t\t\tserviceId : %02" PRIx16 "\n", serviceId);
		}
		break;
	case 0x02 :
		fprintf(stdout, " [link to event]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
		if(xC5->selectorLength==8){
			uint16_t originalNetworkId	= *(xC5->selector) << 8 | *(xC5->selector+1);
			uint16_t transportStreamId	= *(xC5->selector+2) << 8 | *(xC5->selector+3);
			uint16_t serviceId			= *(xC5->selector+4) << 8 | *(xC5->selector+5);
			uint16_t eventId			= *(xC5->selector+6) << 8 | *(xC5->selector+7);
			fprintf(stdout, "\t\t\toriginalNetworkId : %04" PRIx16 "\n", originalNetworkId);
			fprintf(stdout, "\t\t\ttransportStreamId : %04" PRIx16 "\n", transportStreamId);
			fprintf(stdout, "\t\t\tserviceId : %04" PRIx16 " [%" PRId16 "]\n", serviceId, serviceId);
			fprintf(stdout, "\t\t\teventId : %04" PRIx16 " [%" PRId16 "]\n", eventId, eventId);
		}
		break;
	case 0x03 :
		fprintf(stdout, " [link to module]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
		if(xC5->selectorLength==11){
			uint16_t originalNetworkId	= *(xC5->selector) << 8 | *(xC5->selector+1);
			uint16_t transportStreamId	= *(xC5->selector+2) << 8 | *(xC5->selector+3);
			uint16_t serviceId			= *(xC5->selector+4) << 8 | *(xC5->selector+5);
			uint16_t eventId			= *(xC5->selector+6) << 8 | *(xC5->selector+7);
			uint8_t componentTag		= *(xC5->selector+8); 
			uint16_t moduleId			= *(xC5->selector+9) << 8 | *(xC5->selector+10);
			fprintf(stdout, "\t\t\toriginalNetworkId : %04" PRIx16 "\n", originalNetworkId);
			fprintf(stdout, "\t\t\ttransportStreamId : %04" PRIx16 "\n", transportStreamId);
			fprintf(stdout, "\t\t\tserviceId : %04" PRIx16 " [%" PRId16 "]\n", serviceId, serviceId);
			fprintf(stdout, "\t\t\teventId : %04" PRIx16 " [%" PRId16 "]\n", eventId, eventId);
			fprintf(stdout, "\t\t\tcomponentTag : %02" PRIx8 "\n", componentTag);
			fprintf(stdout, "\t\t\tmoduleId : %04" PRIx16 "\n", moduleId);
		}
		break;
	case 0x04 :
		fprintf(stdout, " [link to content]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
		if(xC5->selectorLength==10){
			uint16_t originalNetworkId	= *(xC5->selector) << 8 | *(xC5->selector+1);
			uint16_t transportStreamId	= *(xC5->selector+2) << 8 | *(xC5->selector+3);
			uint16_t serviceId			= *(xC5->selector+4) << 8 | *(xC5->selector+5);
			uint32_t contentId			= *(xC5->selector+6) << 24 | *(xC5->selector+7) << 16 | *(xC5->selector+8) << 8 | *(xC5->selector+9);
			fprintf(stdout, "\t\t\toriginalNetworkId : %04" PRIx16 "\n", originalNetworkId);
			fprintf(stdout, "\t\t\ttransportStreamId : %04" PRIx16 "\n", transportStreamId);
			fprintf(stdout, "\t\t\tserviceId : %04" PRIx16 " [%" PRId16 "]\n", serviceId, serviceId);
			fprintf(stdout, "\t\t\tcontentId : %08" PRIx32 "\n", contentId);
		}
		break;
	case 0x05 :
		fprintf(stdout, " [link to content module]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
		if(xC5->selectorLength==13){
			uint16_t originalNetworkId	= *(xC5->selector) << 8 | *(xC5->selector+1);
			uint16_t transportStreamId	= *(xC5->selector+2) << 8 | *(xC5->selector+3);
			uint16_t serviceId			= *(xC5->selector+4) << 8 | *(xC5->selector+5);
			uint32_t contentId			= *(xC5->selector+6) << 24 | *(xC5->selector+7) << 16 | *(xC5->selector+8) << 8 | *(xC5->selector+9);
			uint8_t componentTag		= *(xC5->selector+10); 
			uint16_t moduleId			= *(xC5->selector+11) << 8 | *(xC5->selector+12);
			fprintf(stdout, "\t\t\toriginalNetworkId : %04" PRIx16 "\n", originalNetworkId);
			fprintf(stdout, "\t\t\ttransportStreamId : %04" PRIx16 "\n", transportStreamId);
			fprintf(stdout, "\t\t\tserviceId : %04" PRIx16 " [%" PRId16 "]\n", serviceId, serviceId);
			fprintf(stdout, "\t\t\tcontentId : %08" PRIx32 "\n", contentId);
			fprintf(stdout, "\t\t\tcomponentTag : %02" PRIx8 "\n", componentTag);
			fprintf(stdout, "\t\t\tmoduleId : %04" PRIx16 "\n", moduleId);
		}
		break;
	case 0x06 :
		fprintf(stdout, " [link to ert node]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
		if(xC5->selectorLength==6){
			uint16_t informationProviderId	= *(xC5->selector) << 8 | *(xC5->selector+1);
			uint16_t eventRelationId	= *(xC5->selector+2) << 8 | *(xC5->selector+3);
			uint16_t nodeId			= *(xC5->selector+4) << 8 | *(xC5->selector+5);
			fprintf(stdout, "\t\t\tinformationProviderId : %04" PRIx16 "\n", informationProviderId);
			fprintf(stdout, "\t\t\teventRelationId : %04" PRIx16 "\n", eventRelationId);
			fprintf(stdout, "\t\t\tnodeId : %02" PRIx16 "\n", nodeId);
		}
		break;
	case 0x07 :
		fprintf(stdout, " [link to stored content]\n"); break;
		fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
		if(xC5->selectorLength>0){
			hex_dump((xC5->selector), xC5->selectorLength, 2);
		}
		uint8_t *sjis = aribTOsjis(xC5->selector, xC5->selectorLength);
		if(sjis!=NULL){
			uint8_t  *p;
			if((p = nkf_convert(sjis, strlen((char *)sjis), (char *)option, strlen(option)))!=NULL){
				fprintf(stdout, "\t\t%s\n", p);
				free(p);
			}
			free(sjis);
		}
		break;
	default :
		// 将来のためリザーブ
		if(xC5->linkDestinationType>=0x08 && xC5->linkDestinationType<=0x7f){
			fprintf(stdout, " [reserved future use]\n");
			fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
			if(xC5->selectorLength>0){
				hex_dump((xC5->selector), xC5->selectorLength, 2);
			}
		// ユーザー定義のリンク先種別
		}else if(xC5->linkDestinationType>=0x80 && xC5->linkDestinationType<=0xfe){
			fprintf(stdout, " [user private]\n");
			fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
			if(xC5->selectorLength>0){
				hex_dump((xC5->selector), xC5->selectorLength, 2);
			}
		// reserved (0xFF)
		}else{
			fprintf(stdout, " [reserved]\n");
			fprintf(stdout, "\t\tselectorLength                 : %02" PRIx8 " [%" PRId8 "]\n", xC5->selectorLength, xC5->selectorLength);
			if(xC5->selectorLength>0){
				hex_dump((xC5->selector), xC5->selectorLength, 2);
			}
		}
		break;
	}

	uint8_t privateDataLength = xC5->descriptorLength-3-xC5->selectorLength;
	if(privateDataLength>0){
		fprintf(stdout, "\t\tprivateDataLength                 : %02" PRIx8 " [%" PRId8 "]\n", privateDataLength, privateDataLength);
		hex_dump((xC5->selector+xC5->selectorLength), privateDataLength, 2);
	}

	return;
}


static void printDescriptorUnKnown(uint8_t *descriptor)
{
	uint8_t descriptorTag = *descriptor;
	uint8_t descriptorLength = *(descriptor+1);

	fprintf(stdout, "\t\tdescriptorTag                  : %02" PRIx8"\n",  descriptorTag);
	fprintf(stdout, "\t\tdescriptorLength               : %02" PRIx8 " [%" PRId8 "]\n", descriptorLength, descriptorLength);
	if(descriptorLength>0){
		hex_dump((descriptor+2), descriptorLength, 2);
	}
	return;
}

static bool printDescriptor(uint8_t descriptorTag, uint8_t *descriptor)
{
	bool rtn = true;

	fprintf(stdout, "\t\t------------------------------------------------------\n");
	fprintf(stdout, "\t\t[DESCRIPTOR : %02" PRIx8"", descriptorTag);
	switch(descriptorTag){
	case 0x4d: // 短形式イベント記述子
		fprintf(stdout, " 短形式イベント記述子]\n");
		{
			DescriptorX4D x4D;
			DescriptorX4D_set(descriptor, &x4D);
			printDescriptorX4D(&x4D);
		}
		break;
	case 0x4e: // 拡張形式イベント記述子
		fprintf(stdout, " 拡張形式イベント記述子]\n");
		{
			DescriptorX4E x4E;
			DescriptorX4E_set(descriptor, &x4E);
			printDescriptorX4E(&x4E);
		}
		break;
	case 0x50: // コンポーネント記述子
		fprintf(stdout, " コンポーネント記述子]\n");
		{
			DescriptorX50 x50;
			DescriptorX50_set(descriptor, &x50);
			printDescriptorX50(&x50);
		}
		break;
	case 0x54: // コンテント記述子
		fprintf(stdout, " コンテント記述子]\n");
		{
			DescriptorX54 x54;
			DescriptorX54_set(descriptor, &x54);
			printDescriptorX54(&x54);
		}
		break;
	case 0x55: // パレンタルレート記述子
		fprintf(stdout, " パレンタルレート記述子]\n");
		{
			DescriptorX55 x55;
			DescriptorX55_set(descriptor, &x55);
			printDescriptorX55(&x55);
		}
		break;
	case 0xc1: // デジタルコピー制御記述子
		fprintf(stdout, " デジタルコピー制御記述子]\n");
		{
			DescriptorXC1 xC1;
			DescriptorXC1_set(descriptor, &xC1);
			printDescriptorXC1(&xC1);
		}
		break;
	case 0xc4: // 音声コンポーネント記述子
		fprintf(stdout, " 音声コンポーネント記述子]\n");
		{
			DescriptorXC4 xC4;
			DescriptorXC4_set(descriptor, &xC4);
			printDescriptorXC4(&xC4);
		}
		break;
	case 0xc7: // データコンテンツ記述子
		fprintf(stdout, " データコンテンツ記述子]\n");
		{
			DescriptorXC7 xC7;
			DescriptorXC7_set(descriptor, &xC7);
			printDescriptorXC7(&xC7);
		}
		break;
	case 0xcb: // CA 契約情報記述子(CA contract info descriptor)
		fprintf(stdout, " CA 契約情報記述子]\n");
		{
			DescriptorXCB xCB;
			DescriptorXCB_set(descriptor, &xCB);
			printDescriptorXCB(&xCB);
		}
		break;
	case 0xd5: // シリーズ記述子
		fprintf(stdout, " シリーズ記述子]\n");
		{
			DescriptorXD5 xD5;
			DescriptorXD5_set(descriptor, &xD5);
			printDescriptorXD5(&xD5);
		}
		break;
	case 0xd6: // イベントグループ記述子
		fprintf(stdout, " イベントグループ記述子]\n");
		{
			DescriptorXD6 xD6;
			DescriptorXD6_set(descriptor, &xD6);
			printDescriptorXD6(&xD6);
		}
		break;
	case 0xd9: // コンポーネントグループ記述子
		fprintf(stdout, " コンポーネントグループ記述子]\n");
		{
			DescriptorXD9 xD9;
			DescriptorXD9_set(descriptor, &xD9);
			printDescriptorXD9(&xD9);
		}
		break;
	case 0xdc: // LDTリンク記述子
		fprintf(stdout, " LDTリンク記述子]\n");
		{
			DescriptorXDC xDC;
			DescriptorXDC_set(descriptor, &xDC);
			printDescriptorXDC(&xDC);
		}
		break;
	case 0x42: // スタッフ記述子
		fprintf(stdout, " スタッフ記述子]\n");
		{
			DescriptorX42 x42;
			DescriptorX42_set(descriptor, &x42);
			printDescriptorX42(&x42);
		}
		break;
	case 0xc5: // DescriptorXC5 ハイパーリンク記述子
		fprintf(stdout, " ハイパーリンク記述子]\n");
		{
			DescriptorXC5 xC5;
			DescriptorXC5_set(descriptor, &xC5);
			printDescriptorXC5(&xC5);
		}
		break;
	default:
		fprintf(stdout, " ???]\n");
		printDescriptorUnKnown(descriptor);
		rtn = false;
		break;
	}

	return(rtn);
}


int main(int argc, char *argv[])
{

	uint8_t payload[MAX_PAYLOAD];
	size_t	payload_len;
	FILE *fp;
	bool find;
	EIT eit;
	EitDescriptor edesc;
	ARG_PARAM param;

	param.pid = 0xffff;
	param.sid = 0xffff;
	if(parseOption(argc, argv, &param)){
		fprintf(stdout,"parseOption() return true\n");
		fprintf(stdout,"pid     = %d\n", param.pid);
		fprintf(stdout,"sid     = %d\n", param.sid);
		fprintf(stdout,"file    = %s\n", param.file);

		// 未指定時、デフォルト0x12とする
		if(param.pid==0xffff){
			param.pid = 0x12;
		}
	}else{
		fprintf(stdout,"parseOption() return false\n");
		return(-1);
	}

	if((fp=fopen(param.file,"r"))==NULL){
		fprintf(stderr, "file open error : %s\n", param.file);
		return(-1);
   	 }

	while(!feof(fp)){
		find = create_payload(param.pid, payload, &payload_len, &fp);
		if(find){
			memset(&eit, '\0', sizeof(EIT));
			EIT_set(payload, &eit);

			if(param.sid==0xffff || param.sid == eit.serviceId){
				printEIT(&eit);
			}

			memset(&edesc, '\0', sizeof(EitDescriptor));
			for(int eDescriptorLength=0; eDescriptorLength<eit.sectionLength-11-4; eDescriptorLength+=12+edesc.descriptorsLoopLength){
																				// 11 : EIT serviceId から lastTableId までのbyte数
																				// 4  : EIT CRC32 のbyte数
																				// eit.sectionLength-11-4 : EIT sectionData のbyte数
																				// 12 : eventId から descriptorsLoopLength までのbyte数
																				// edesc.descriptorsLoopLength : descriptor のbyte数
																				// 12+sdesc.descriptorsLoopLength : EIT Descriptor 1つのbyte数

				memset(&edesc, '\0', sizeof(EitDescriptor));
				EitDescriptor_set(eit.sectionData+eDescriptorLength, &edesc);
				uint8_t descriptorTag;
				if(param.sid==0xffff || param.sid == eit.serviceId){
					printEitDescriptor(&edesc);
				}

				for(int descriptorOffset=0; descriptorOffset<edesc.descriptorsLoopLength; descriptorOffset+=*(edesc.descriptor+descriptorOffset+1) + 2){
																				// *(edesc.descriptor+descriptorOffset+1) : descriptorLength
																				// 2 : descriptorTag 1byte + descriptorLength 1byte
																				// *(edesc.descriptor+descriptorOffset+1) + 2はdescriptor1つのbyte数
					descriptorTag = *(edesc.descriptor+descriptorOffset);


					if(param.sid==0xffff || param.sid == eit.serviceId){
						if(!printDescriptor(descriptorTag, edesc.descriptor+descriptorOffset)){
							break;
						}
					}
				}
			}
		}
	}
	fclose(fp);

	return(0);
}


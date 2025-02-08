//345678901234567890123456789012345678901234567890123456789012345678901234567890
/******************************************************************************/
/* 関数名：aribTOsjis                                                         */
/* 機能  ：ARIB文字コードをSJISに変換する                                     */
/*           但し、ASCIIひらがなカタカナ漢字のみとし、ARIB外字には対応しない  */
/*                                                                            */
/* uint8_t *aribTOsjis(uint8_t *input, size_t length)                         */
/*            input :ARIB文字列                                               */
/*            length:ARIB文字列byte数                                         */
/*            戻値  :SJIS文字列先頭アドレス 終端文字 '\0'                     */
/*            注意  :アロケートメモリなので使用後freeすること                 */
/*                                                                            */
/******************************************************************************/
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

	// ARIB ひらがな SJIS 全角ひらがな変換テーブル ([0]:ARIB Code, [1-2]:SJIS 2Byte)
	const uint8_t hiraganaTable[][3] = {
			{0x20, 0x81, 0x40}, //"　"
			{0x21, 0x82, 0x9F}, // ぁ
			{0x22, 0x82, 0xA0}, // あ
			{0x23, 0x82, 0xA1}, // ぃ
			{0x24, 0x82, 0xA2}, // い
			{0x25, 0x82, 0xA3}, // ぅ
			{0x26, 0x82, 0xA4}, // う
			{0x27, 0x82, 0xA5}, // ぇ
			{0x28, 0x82, 0xA6}, // え
			{0x29, 0x82, 0xA7}, // ぉ
			{0x2A, 0x82, 0xA8}, // お
			{0x2B, 0x82, 0xA9}, // か
			{0x2C, 0x82, 0xAA}, // が
			{0x2D, 0x82, 0xAB}, // き
			{0x2E, 0x82, 0xAC}, // ぎ
			{0x2F, 0x82, 0xAD}, // く
			{0x30, 0x82, 0xAE}, // ぐ
			{0x31, 0x82, 0xAF}, // け
			{0x32, 0x82, 0xB0}, // げ
			{0x33, 0x82, 0xB1}, // こ
			{0x34, 0x82, 0xB2}, // ご
			{0x35, 0x82, 0xB3}, // さ
			{0x36, 0x82, 0xB4}, // ざ
			{0x37, 0x82, 0xB5}, // し
			{0x38, 0x82, 0xB6}, // じ
			{0x39, 0x82, 0xB7}, // す
			{0x3A, 0x82, 0xB8}, // ず
			{0x3B, 0x82, 0xB9}, // せ
			{0x3C, 0x82, 0xBA}, // ぜ
			{0x3D, 0x82, 0xBB}, // そ
			{0x3E, 0x82, 0xBC}, // ぞ
			{0x3F, 0x82, 0xBD}, // た
			{0x40, 0x82, 0xBE}, // だ
			{0x41, 0x82, 0xBF}, // ち
			{0x42, 0x82, 0xC0}, // ぢ
			{0x43, 0x82, 0xC1}, // っ
			{0x44, 0x82, 0xC2}, // つ
			{0x45, 0x82, 0xC3}, // づ
			{0x46, 0x82, 0xC4}, // て
			{0x47, 0x82, 0xC5}, // で
			{0x48, 0x82, 0xC6}, // と
			{0x49, 0x82, 0xC7}, // ど
			{0x4A, 0x82, 0xC8}, // な
			{0x4B, 0x82, 0xC9}, // に
			{0x4C, 0x82, 0xCA}, // ぬ
			{0x4D, 0x82, 0xCB}, // ね
			{0x4E, 0x82, 0xCC}, // の
			{0x4F, 0x82, 0xCD}, // は
			{0x50, 0x82, 0xCE}, // ば
			{0x51, 0x82, 0xCF}, // ぱ
			{0x52, 0x82, 0xD0}, // ひ
			{0x53, 0x82, 0xD1}, // び
			{0x54, 0x82, 0xD2}, // ぴ
			{0x55, 0x82, 0xD3}, // ふ
			{0x56, 0x82, 0xD4}, // ぶ
			{0x57, 0x82, 0xD5}, // ぷ
			{0x58, 0x82, 0xD6}, // へ
			{0x59, 0x82, 0xD7}, // べ
			{0x5A, 0x82, 0xD8}, // ぺ
			{0x5B, 0x82, 0xD9}, // ほ
			{0x5C, 0x82, 0xDA}, // ぼ
			{0x5D, 0x82, 0xDB}, // ぽ
			{0x5E, 0x82, 0xDC}, // ま
			{0x5F, 0x82, 0xDD}, // み
			{0x60, 0x82, 0xDE}, // む
			{0x61, 0x82, 0xDF}, // め
			{0x62, 0x82, 0xE0}, // も
			{0x63, 0x82, 0xE1}, // ゃ
			{0x64, 0x82, 0xE2}, // や
			{0x65, 0x82, 0xE3}, // ゅ
			{0x66, 0x82, 0xE4}, // ゆ
			{0x67, 0x82, 0xE5}, // ょ
			{0x68, 0x82, 0xE6}, // よ
			{0x69, 0x82, 0xE7}, // ら
			{0x6A, 0x82, 0xE8}, // り
			{0x6B, 0x82, 0xE9}, // る
			{0x6C, 0x82, 0xEA}, // れ
			{0x6D, 0x82, 0xEB}, // ろ
			{0x6E, 0x82, 0xEC}, // ゎ
			{0x6F, 0x82, 0xED}, // わ
			{0x70, 0x82, 0xEE}, // ゐ
			{0x71, 0x82, 0xEF}, // ゑ
			{0x72, 0x82, 0xF0}, // を
			{0x73, 0x82, 0xF1}, // ん
			{0x74, 0x81, 0x40}, //"　"
			{0x75, 0x81, 0x40}, //"　"
			{0x76, 0x81, 0x40}, //"　"
			{0x77, 0x81, 0x54}, // ゝ
			{0x78, 0x81, 0x55}, // ゞ
			{0x79, 0x81, 0x5B}, // ー
			{0x7A, 0x81, 0x42}, // 。
			{0x7B, 0x81, 0x75}, // 「
			{0x7C, 0x81, 0x76}, // 」
			{0x7D, 0x81, 0x41}, // 、
			{0x7E, 0x81, 0x45}, // ・
			{0x7F, 0x81, 0x40}  //"　"
		};


	// ARIB カタカナ SJIS 全角カタカナ変換テーブル ([0]:ARIB Code, [1-2]:SJIS 2Byte)
	const uint8_t katakanaTable[][3] = {
			{0x20, 0x81, 0x40}, //"　"
			{0x21, 0x83, 0x40}, // ァ
			{0x22, 0x83, 0x41}, // ア
			{0x23, 0x83, 0x42}, // ィ
			{0x24, 0x83, 0x43}, // イ
			{0x25, 0x83, 0x44}, // ゥ
			{0x26, 0x83, 0x45}, // ウ
			{0x27, 0x83, 0x46}, // ェ
			{0x28, 0x83, 0x47}, // エ
			{0x29, 0x83, 0x48}, // ォ
			{0x2A, 0x83, 0x49}, // オ
			{0x2B, 0x83, 0x4A}, // カ
			{0x2C, 0x83, 0x4B}, // ガ
			{0x2D, 0x83, 0x4C}, // キ
			{0x2E, 0x83, 0x4D}, // ギ
			{0x2F, 0x83, 0x4E}, // ク
			{0x30, 0x83, 0x4F}, // グ
			{0x31, 0x83, 0x50}, // ケ
			{0x32, 0x83, 0x51}, // ゲ
			{0x33, 0x83, 0x52}, // コ
			{0x34, 0x83, 0x53}, // ゴ
			{0x35, 0x83, 0x54}, // サ
			{0x36, 0x83, 0x55}, // ザ
			{0x37, 0x83, 0x56}, // シ
			{0x38, 0x83, 0x57}, // ジ
			{0x39, 0x83, 0x58}, // ス
			{0x3A, 0x83, 0x59}, // ズ
			{0x3B, 0x83, 0x5A}, // セ
			{0x3C, 0x83, 0x5B}, // ゼ
			{0x3D, 0x83, 0x5C}, // ソ
			{0x3E, 0x83, 0x5D}, // ゾ
			{0x3F, 0x83, 0x5E}, // タ
			{0x40, 0x83, 0x5F}, // ダ
			{0x41, 0x83, 0x60}, // チ
			{0x42, 0x83, 0x61}, // ヂ
			{0x43, 0x83, 0x62}, // ッ
			{0x44, 0x83, 0x63}, // ツ
			{0x45, 0x83, 0x64}, // ヅ
			{0x46, 0x83, 0x65}, // テ
			{0x47, 0x83, 0x66}, // デ
			{0x48, 0x83, 0x67}, // ト
			{0x49, 0x83, 0x68}, // ド
			{0x4A, 0x83, 0x69}, // ナ
			{0x4B, 0x83, 0x6A}, // ニ
			{0x4C, 0x83, 0x6B}, // ヌ
			{0x4D, 0x83, 0x6C}, // ネ
			{0x4E, 0x83, 0x6D}, // ノ
			{0x4F, 0x83, 0x6E}, // ハ
			{0x50, 0x83, 0x6F}, // バ
			{0x51, 0x83, 0x70}, // パ
			{0x52, 0x83, 0x71}, // ヒ
			{0x53, 0x83, 0x72}, // ビ
			{0x54, 0x83, 0x73}, // ピ
			{0x55, 0x83, 0x74}, // フ
			{0x56, 0x83, 0x75}, // ブ
			{0x57, 0x83, 0x76}, // プ
			{0x58, 0x83, 0x77}, // ヘ
			{0x59, 0x83, 0x78}, // ベ
			{0x5A, 0x83, 0x79}, // ペ
			{0x5B, 0x83, 0x7A}, // ホ
			{0x5C, 0x83, 0x7B}, // ボ
			{0x5D, 0x83, 0x7C}, // ポ
			{0x5E, 0x83, 0x7D}, // マ
			{0x5F, 0x83, 0x7E}, // ミ
			{0x60, 0x83, 0x80}, // ム
			{0x61, 0x83, 0x81}, // メ
			{0x62, 0x83, 0x82}, // モ
			{0x63, 0x83, 0x83}, // ャ
			{0x64, 0x83, 0x84}, // ヤ
			{0x65, 0x83, 0x85}, // ュ
			{0x66, 0x83, 0x86}, // ユ
			{0x67, 0x83, 0x87}, // ョ
			{0x68, 0x83, 0x88}, // ヨ
			{0x69, 0x83, 0x89}, // ラ
			{0x6A, 0x83, 0x8A}, // リ
			{0x6B, 0x83, 0x8B}, // ル
			{0x6C, 0x83, 0x8C}, // レ
			{0x6D, 0x83, 0x8D}, // ロ
			{0x6E, 0x83, 0x8E}, // ヮ
			{0x6F, 0x83, 0x8F}, // ワ
			{0x70, 0x83, 0x90}, // ヰ
			{0x71, 0x83, 0x91}, // ヱ
			{0x72, 0x83, 0x92}, // ヲ
			{0x73, 0x83, 0x93}, // ン
			{0x74, 0x83, 0x94}, // ヴ
			{0x75, 0x83, 0x95}, // ヵ
			{0x76, 0x83, 0x96}, // ヶ
			{0x77, 0x81, 0x52}, // ヽ
			{0x78, 0x81, 0x53}, // ヾ
			{0x79, 0x81, 0x5B}, // ー
			{0x7A, 0x81, 0x42}, // 。
			{0x7B, 0x81, 0x75}, // "「"
			{0x7C, 0x81, 0x76}, // "」"
			{0x7D, 0x81, 0x41}, // "、"
			{0x7E, 0x81, 0x45}, // "・"
			{0x7F, 0x81, 0x40}  // "　"
		};

// 符号集合の分類
// Gセット(文字系)
#define GSET_KANJI					0x42 // 2バイト符号 実質JIS X 0208 漢字
#define GSET_ASCII					0x4A // 1バイト符号 ARIBのASCIIコード表
#define GSET_HIRAGANA				0x30 // 1バイト符号 ARIBの平仮名コード表
#define GSET_KATAKANA 				0x31 // 1バイト符号 ARIBのカタカナコード表
#define GSET_MOSAIC_A 				0x32 // 1バイト符号
#define GSET_MOSAIC_B 				0x33 // 1バイト符号
#define GSET_MOSAIC_C 				0x34 // 1バイト符号
#define GSET_MOSAIC_D 				0x35 // 1バイト符号

#define GSET_P_ASCII				0x36 // 1バイト符号 可変幅フォント 英数
#define GSET_P_HIRAGANA				0x37 // 1バイト符号 可変幅フォント 平仮名
#define GSET_P_KATAKANA				0x38 // 1バイト符号 可変幅フォント カタカナ

#define GSET_JIS_X0201_KATAKANA		0x49 // 1バイト符号 ARIBのJIS X0201片仮名コード表
#define GSET_JIS_COMPATIBLE_KANJI1	0x39 // 2バイト符号 JIS互換漢字1面 実質JIS X 0213
#define GSET_JIS_COMPATIBLE_KANJI2	0x3A // 2バイト符号 JIS互換漢字2面 実質JIS X 0213

#define GSET_ADD_CODE				0x3B // 2バイト符号 追加記号 ARIBの追加記号を参照

// DRCS(図形など) ※  今回は未使用
#define DRCS_0		0x40 // 2バイト符号	　	　
#define DRCS_1		0x41 // 1バイト符号	　	　
#define DRCS_2		0x42 // 1バイト符号	　	　
#define DRCS_3		0x43 // 1バイト符号	　	　
#define DRCS_4		0x44 // 1バイト符号	　	　
#define DRCS_5		0x45 // 1バイト符号	　	　
#define DRCS_6		0x46 // 1バイト符号	　	　
#define DRCS_7		0x47 // 1バイト符号	　	　
#define DRCS_8		0x48 // 1バイト符号	　	　
#define DRCS_9		0x49 // 1バイト符号	　	　
#define DRCS_10		0x4A // 1バイト符号	　	　
#define DRCS_11		0x4B // 1バイト符号	　	　
#define DRCS_12		0x4C // 1バイト符号	　	　
#define DRCS_13		0x4D // 1バイト符号	　	　
#define DRCS_14		0x4E // 1バイト符号	　	　
#define DRCS_15		0x4F // 1バイト符号	　	　
#define DRCS_MACRO	0x70 // 1バイト符号


// コマンド
// 中間バッファ(G0～G3)に文字テーブルを割り当てる

	// データ数が1byteで表現されるGセットをG0-G3に割り当てる
	const uint8_t	D1GSETtoG0[] = {0x1B, 0x28};	// GSET -> G0
	const uint8_t	D1GSETtoG1[] = {0x1B, 0x29};	// GSET -> G1
	const uint8_t	D1GSETtoG2[] = {0x1B, 0x2A};	// GSET -> G2
	const uint8_t	D1GSETtoG3[] = {0x1B, 0x2B};	// GSET -> G3

	// データ数が1byteで表現されるDRCSをG0-G3に割り当てる
	const uint8_t	D1DRCStoG0[] = {0x1B, 0x28, 0x20};	// DRCS -> G0
	const uint8_t	D1DRCStoG1[] = {0x1B, 0x29, 0x20};	// DRCS -> G1
	const uint8_t	D1DRCStoG2[] = {0x1B, 0x2A, 0x20};	// DRCS -> G2
	const uint8_t	D1DRCStoG3[] = {0x1B, 0x2B, 0x20};	// DRCS -> G3

	// データ数が2byteで表現されるGセットをG0に割り当てる
	const uint8_t	D2GSETtoG0[] = {0x1B, 0x24};		// GSET -> G0
	const uint8_t	D2GSETtoG1[] = {0x1B, 0x24, 0x29};	// GSET -> G1
	const uint8_t	D2GSETtoG2[] = {0x1B, 0x24, 0x2A};	// GSET -> G2
	const uint8_t	D2GSETtoG3[] = {0x1B, 0x24, 0x2B};	// GSET -> G3

	// データ数が2byteで表現されるDRCSをG0に割り当てる
	const uint8_t	D2DRCStoG0[] = {0x1B, 0x28, 0x20};	// DRCS -> G0
	const uint8_t	D2DRCStoG1[] = {0x1B, 0x29, 0x20};	// DRCS -> G1
	const uint8_t	D2DRCStoG2[] = {0x1B, 0x2A, 0x20};	// DRCS -> G2
	const uint8_t	D2DRCStoG3[] = {0x1B, 0x2B, 0x20};	// DRCS -> G3

	// 中間バッファをGL、GRに割り当てる
	// (次に割り当てが変更されるまで保持する)
	const uint8_t	G0toGL = 0x0F;				// LS0 G0をGLに割り当てる
	const uint8_t	G1toGL = 0x0E;				// LS1 G1をGLに割り当てる
	const uint8_t	G2toGL[] = {0x1B, 0x6E};	// LS2 G2をGLに割り当てる
	const uint8_t	G3toGL[] = {0x1B, 0x6F};	// LS3 G3をGLに割り当てる
	const uint8_t	G1toGR[] = {0x1B, 0x7E};	// LS1R G1をGRに割り当てる
	const uint8_t	G2toGR[] = {0x1B, 0x7D};	// LS2R G2をGRに割り当てる
	const uint8_t	G3toGR[] = {0x1B, 0x7C};	// LS3R G3をGRに割り当てる
	// (次のデータを1度だけ処理したら以前の中間バッファに戻す)
	const uint8_t	G2toGL_ONCE = 0x19;			// SS2 次のデータを1度だけ処理したら以前の中間バッファに戻す
	const uint8_t	G3toGL_ONCE = 0x1D;			// SS3 次のデータを1度だけ処理したら以前の中間バッファに戻す



/******************************************************************************/
/* 内部関数                                                                   */
/* static bool gsetCheck(uint8_t gset)                                        */
/* GSETが変換可能かチェックする                                               */
/******************************************************************************/
static bool gsetCheck(uint8_t gset){

	bool rtn;
	switch(gset){
		case GSET_KANJI:					// 2バイト符号 実質JIS X 0208 漢字
		case GSET_ASCII:					// 1バイト符号 ARIBのASCIIコード表
		case GSET_HIRAGANA:					// 1バイト符号 ARIBの平仮名コード表
		case GSET_KATAKANA: 				// 1バイト符号 ARIBのカタカナコード表

		case GSET_P_ASCII:					// 1バイト符号 可変幅フォント 英数
		case GSET_P_HIRAGANA:				// 1バイト符号 可変幅フォント 平仮名
		case GSET_P_KATAKANA:				// 1バイト符号 可変幅フォント カタカナ

		case GSET_JIS_X0201_KATAKANA:		// 1バイト符号 ARIBのJIS X0201片仮名コード表
		case GSET_JIS_COMPATIBLE_KANJI1:	// 2バイト符号 JIS互換漢字1面 実質JIS X 0213
		case GSET_JIS_COMPATIBLE_KANJI2:	// 2バイト符号 JIS互換漢字2面 実質JIS X 0213

		case GSET_ADD_CODE:					// 2バイト符号 追加記号 ARIBの追加記号を参照
			rtn = true;
			break;
		default:
			rtn = false;
			break;
	}

		return(rtn);
}

/******************************************************************************/
/* 関数名：aribTOsjis                                                         */
/* 機能  ：ARIB文字コードをSJISに変換する                                     */
/*           但し、ASCIIひらがなカタカナ漢字のみとし、ARIB外字には対応しない  */
/*                                                                            */
/* char *aribTOsjis(uint8_t *input, size_t length)                            */
/*            input :ARIB文字列                                               */
/*            length:ARIB文字列byte数                                         */
/*            戻値  :SJIS文字列先頭アドレス 終端文字 '\0'                     */
/*            注意  :アロケートメモリなので使用後freeすること                 */
/*                                                                            */
/******************************************************************************/
uint8_t *aribTOsjis(uint8_t *input, size_t length)
{
	uint8_t *sjis;

	// 中間バッファ デフォルト文字テーブル
	uint8_t G0 = GSET_KANJI;	// 漢字
	uint8_t G1 = GSET_ASCII;	// ASCII
	uint8_t G2 = GSET_HIRAGANA;	// 平仮名
	uint8_t G3 = GSET_KATAKANA;	// カタカナ

	uint8_t GL = G0;
	uint8_t GR = G2;

	int8_t beforeBuf = -1;

//	bool char2Byte = true;	// true:2byte/false:1byte

	if( (sjis = (uint8_t *)malloc(length*2+1))==NULL){
		return(NULL);
	}
	memset(sjis, '\0', length*2+1);

	int offSet = 0;
	while(offSet < length){

		/************************/
		/* GL GR 割当コマンド	*/
		/************************/
		// 使わない制御コードを読み飛ばす
		if( *(input+offSet)==0x00 ||
			*(input+offSet)==0x16 ||
			*(input+offSet)==0x18 ||
			*(input+offSet)==0x1C ||
			*(input+offSet)==0x1E ||
			*(input+offSet)==0x1F ||
			*(input+offSet)==0x20 ||
			*(input+offSet)==0x7f ||
			*(input+offSet)==0xA0 ||
			*(input+offSet)==0xFF ||
			(*(input+offSet)>=0x07 && *(input+offSet)<=0x0D) ||
			(*(input+offSet)>=0x80 && *(input+offSet)<=0x8B) ||
			(*(input+offSet)>=0x90 && *(input+offSet)<=0x9D)){
			offSet += 1;
			continue;
		}

		//  GL に中間バッファ G0-G3 をセット
		// 次の文字を処理する為に1byte シフト
		// ループ先頭に戻す
		if(*(input+offSet) == G0toGL){
			GL = G0;
			offSet += 1;
			continue;
		}else if(*(input+offSet) == G1toGL){
			GL = G1;										
			offSet += 1;
			continue;
		}else if(*(input+offSet) == G2toGL_ONCE){
			beforeBuf = GL;
			GL = G2;
			offSet += 1;
			continue;
		}else if(*(input+offSet) == G3toGL_ONCE){
			beforeBuf = GL;
			GL = G3;
			offSet += 1;
			continue;

		}else if(*(input+offSet) == 0x1B){				// 制御コードなのでコマンド

			if(!memcmp(G2toGL, input+offSet, sizeof(G2toGL))){
				GL = G2;									// G2をGLに割り当てる
				offSet += sizeof(G2toGL);					// 次の文字を処理する為に2byte シフト
			}else if(!memcmp(G3toGL, input+offSet, sizeof(G3toGL))){
				GL = G3;									// G3をGLに割り当てる
				offSet += sizeof(G3toGL);					// 次の文字を処理する為に2byte シフト
			}else if(!memcmp(G1toGR, input+offSet, sizeof(G1toGR))){
				GR = G1;									// G1をGRに割り当てる
				offSet += sizeof(G1toGR);					// 次の文字を処理する為に2byte シフト
			}else if(!memcmp(G2toGR, input+offSet, sizeof(G2toGR))){
				GR = G2;									// G2をGRに割り当てる
				offSet += sizeof(G2toGR);					// 次の文字を処理する為に2byte シフト
			}else if(!memcmp(G3toGR, input+offSet, sizeof(G3toGR))){
				GR = G3;									// コマンド:G3をGRに割り当てる
				offSet += sizeof(G3toGR);					// 次の文字を処理する為に2byte シフト

			/****************************/
			/* 中間バッファ割当コマンド	*/
			/****************************/

			// 制御コードをチェックし、適合した場合、その直後の1byteを終端符号としてG0-G3にセットする
			// 但し処理不能な制御コードは対応しない
			// 1byte 文字制御
			}else if(!memcmp(D1GSETtoG0, input+offSet, sizeof(D1GSETtoG0))){
				if(gsetCheck(*(input+offSet+sizeof(D1GSETtoG0)))){ G0 = *(input+offSet+sizeof(D1GSETtoG0));}
				offSet += sizeof(D1GSETtoG0) + 1;				

			}else if(!memcmp(D1GSETtoG1, input+offSet, sizeof(D1GSETtoG1))){
				if(gsetCheck(*(input+offSet+sizeof(D1GSETtoG1)))){ G1 = *(input+offSet+sizeof(D1GSETtoG1));}
				G1 = *(input+offSet+sizeof(D1GSETtoG1));
				offSet += sizeof(D1GSETtoG1) + 1;

			}else if(!memcmp(D1GSETtoG2, input+offSet, sizeof(D1GSETtoG2))){
				if(gsetCheck(*(input+offSet+sizeof(D1GSETtoG2)))){ G2 = *(input+offSet+sizeof(D1GSETtoG2));}
				offSet += sizeof(D1GSETtoG2) + 1;

			}else if(!memcmp(D1GSETtoG3, input+offSet, sizeof(D1GSETtoG3))){
				if(gsetCheck(*(input+offSet+sizeof(D1GSETtoG3)))){ G3 = *(input+offSet+sizeof(D1GSETtoG3));}
				offSet += sizeof(D1GSETtoG3)+ 1;

			// 2byte 文字制御

			}else if(!memcmp(D2GSETtoG0, input+offSet, sizeof(D2GSETtoG0))){
				if(gsetCheck(*(input+offSet+sizeof(D2GSETtoG0)))){ G0 = *(input+offSet+sizeof(D2GSETtoG0));}
				offSet += sizeof(D2GSETtoG0) + 1;

			}else if(!memcmp(D2GSETtoG1, input+offSet, sizeof(D2GSETtoG1))){
				if(gsetCheck(*(input+offSet+sizeof(D2GSETtoG1)))){ G1 = *(input+offSet+sizeof(D2GSETtoG1));}
				offSet += sizeof(D2GSETtoG1) + 1;

			}else if(!memcmp(D2GSETtoG2, input+offSet, sizeof(D2GSETtoG2))){
				if(gsetCheck(*(input+offSet+sizeof(D2GSETtoG2)))){ G2 = *(input+offSet+sizeof(D2GSETtoG2));}
				offSet += sizeof(D2GSETtoG2) + 1;

			}else if(!memcmp(D2GSETtoG3, input+offSet, sizeof(D2GSETtoG3))){
				if(gsetCheck(*(input+offSet+sizeof(D2GSETtoG3)))){ G3 = *(input+offSet+sizeof(D2GSETtoG3));}
				offSet += sizeof(D2GSETtoG3) + 1;

			}else{
				// 1B(ESC)の後続に当てはまる制御コードがなければ読み捨てる
				offSet += 1;
			}
					
			continue;
		}

		/************************************************************************/
		/* 文字データ処理														*/
		/* 制御系データの場合、continueでループ頭に戻すのでここには入ってこない	*/
		/************************************************************************/

		if( (*(input+offSet) >= 0x20) &&
			(*(input+offSet) <= 0x7F) ){
			switch(GL){
				uint8_t henkan[2];
				case GSET_KANJI:	// 漢字
				case GSET_JIS_COMPATIBLE_KANJI1:
				case GSET_JIS_COMPATIBLE_KANJI2:
				// 漢字変換処理
					henkan[0] = (*(input+offSet)-0x21) / 2 + ((*(input+offSet)<=0x5E) ? 0x81 : 0xC1);
					if(((*(input+offSet)) & 0x01) == 1){
						henkan[1] = *(input+offSet+1) + ((*(input+offSet+1)<=0x5F) ? 0x1F : 0x20);
					}else{
						henkan[1] = *(input+offSet+1) + 0x7E;
					}
					memcpy(sjis+strlen((char *)sjis), henkan, 2);
					offSet += 2;
					break;

				case GSET_ASCII:	// ASCII
				case GSET_P_ASCII:
				// 編集バッファに追加する
					memcpy(sjis+strlen((char *)sjis), input+offSet, 1);
					offSet += 1;
					break;

				case GSET_HIRAGANA:	// 平仮名
				case GSET_P_HIRAGANA:
				// 対応するSJISコードを編集バッファに追加する
					henkan[0] = hiraganaTable[*(input+offSet)-0x20][1];
					henkan[1] = hiraganaTable[*(input+offSet)-0x20][2];
					memcpy(sjis+strlen((char *)sjis), henkan, 2);
					offSet += 1;
					break;

				case GSET_KATAKANA:	// カタカナ
				case GSET_JIS_X0201_KATAKANA:
				case GSET_P_KATAKANA:
				// 対応するSJISコードを編集バッファに追加する
					henkan[0] = katakanaTable[*(input+offSet)-0x20][1];
					henkan[1] = katakanaTable[*(input+offSet)-0x20][2];
					memcpy(sjis+strlen((char *)sjis), henkan, 2);
					offSet += 1;
					break;

				case GSET_ADD_CODE:	// 追加記号 (2byte)
				// 文字化け防止用に ？ をセットする
					henkan[0] = 0x81;
					henkan[1] = 0x48;
					memcpy(sjis+strlen((char *)sjis), henkan, 2);
					offSet += 2;
					break;
			}

			if(beforeBuf != -1){
				GL = beforeBuf;
				beforeBuf = -1;
			}


		}else if(*(input+offSet) > 0x7F){
			switch(GR){
				uint8_t henkan[2];
				case GSET_ASCII:	// ASCII
				case GSET_P_ASCII:
					// 最上位ビットを0にして編集バッファに追加する
					henkan[0] = *(input+offSet) & 0x7F;
					memcpy(sjis+strlen((char *)sjis), henkan, 1);
					offSet += 1;
					break;

				case GSET_HIRAGANA:	// 平仮名
				case GSET_P_HIRAGANA:
					// 最上位ビットを0にして対応するSJISコードを編集バッファに追加する
					henkan[0] = hiraganaTable[(*(input+offSet)&0x7F)-0x20][1];
					henkan[1] = hiraganaTable[(*(input+offSet)&0x7F)-0x20][2];
					memcpy(sjis+strlen((char *)sjis), henkan, 2);
					offSet += 1;
					break;

				case GSET_KATAKANA:	// カタカナ
				case GSET_P_KATAKANA:
				case GSET_JIS_X0201_KATAKANA:
					// 最上位ビットを0にして対応するSJISコードを編集バッファに追加する
					henkan[0] = katakanaTable[(*(input+offSet)&0x7F)-0x20][1];
					henkan[1] = katakanaTable[(*(input+offSet)&0x7F)-0x20][2];
					memcpy(sjis+strlen((char *)sjis), henkan, 2);
					offSet += 1;
					break;

				case GSET_ADD_CODE:	// 追加記号 (2byte)
					// 文字化け防止用に ？ をセットする
					henkan[0] = 0x81;
					henkan[1] = 0x48;
					memcpy(sjis+strlen((char *)sjis), henkan, 2);
					offSet += 2;
					break;
			}
			if(beforeBuf != -1){
				GL = beforeBuf;
				beforeBuf = -1;
			}
		}else{
			// 当てはまらなかった文字は読み捨てる
			offSet += 1;
		}
	}

	return(sjis);
}


# LinuxでTV視聴録画環境構築、開発する際に使用するツール

-  **[bcas_ex]**  
   BCAS-ID(0000-1111-2222-333)を 6byte+チェックサム2byteバイナリに変換、または6byteバイナリをBCAS-IDに変換するツール  
   使用方法：  
   $ ./bcas_ex 0000-1111-2222-333  or ./bcas_ex 0123456789ab
-  **[cvi_scan]**  
  TSファイル内SDTからCVIを抽出するツール  
  使用方法：  
  TS抜きチューナーを使用し、 NHK-BS1 (networkID 4) ショップチャンネル (networkID 6) QVC (networkID 7)  
  をそれぞれ3分間程度録画しTSファイルとして保存する  
  $ ./cvi_scan -x BS1.ts SC.ts QVC.ts  
  xmlフォーマットでCVIを標準出力する  
-  **[eit_scan]**  
  TSファイル内にあるEITをダンプ出力するツール  
  イベント事の記述子を全て出力する  
  使用方法：  
  $ ./eit_scan --pid 999 --sid 999 --file file path  
      \--pid   PID(0x12 or 0x26 or 0x27 PID オプション省略時は0x12がデフォルト)  
      \--sid   指定したSIDのEITのみ出力  
      \--file  TSファイル名を指定
  注：TSファイルはEDCBで作成したEPGファイルでも可能
-  **[ts_dump]**  
  mpeg2-TSファイル (1packet 188byte) をダンプ出力するツール  
  使用方法：  
  $ ./ts_dump [-p pid -s] tsfile  
      \-p  指定したPIDのみダンプ出力する  
      \-s  simpleモードで出力する  

  

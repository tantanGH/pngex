# pngex
PNG image loader for X680x0 with XEiJ graphic extension (Preview)

X680x0用のPNG画像ローダです。[XEiJ](https://stdkmd.net/xeij/)の[拡張グラフィック画面](https://stdkmd.net/xeij/feature.htm#extendedgraphic)に対応しており、最大1024x1024x32768色の表示が可能です。それ以上のサイズはクリップされます。

実機([X68000 Z](https://www.zuiki.co.jp/products/x68000z/)含む)や他のX68エミュレータでは最大512x512にクリップされます。


プレビュー版につき一部オプションの挙動が未実装だったり怪しかったりします。


以下のライブラリを使用させて頂きました。

- [zlib 1.2.23](https://github.com/madler/zlib)

リソース消費の大きいlibpngは使わず、シンプルに24bitRGBまたは32bitRGBAのPNGの自力デコードのみ対応しています。

また、コンパイル・アセンブル・リンクには M1 Mac上で以下のクロス開発環境を利用させて頂きました。

- [xdev68k](https://github.com/yosshin4004/xdev68k)
- [Visual Studio Code](https://code.visualstudio.com/)

なお、ここにPNGEX.Xをコンパイルするためには別途 libz.a および zlib のインクルードファイルが必要です。
Makefileはxdev68k付属のサンプルをほぼそのまま使わせて頂いています。

参考文献など

 - InsideX68000 (桑野雅彦, 1992年, ソフトバンク)
 - BMPL.X のソース (Arimac氏)

この場を借りてお礼申し上げます。


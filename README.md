# PNGEX.X

PNG image loader for X680x0 with XEiJ graphic extension support

X680x0用のPNG画像ローダです。[XEiJ](https://stdkmd.net/xeij/)の[拡張グラフィック画面](https://stdkmd.net/xeij/feature.htm#extendedgraphic)に対応しており、最大1024x1024x32768色の表示が可能です。それ以上のサイズはクリップされます。

実機([X68000 Z](https://www.zuiki.co.jp/products/x68000z/)含む)や他のX68エミュレータでは最大512x512にクリップされます。


プレビュー版につき一部オプションの挙動が未実装だったり怪しかったりします。


以下のライブラリを使用させて頂きました。

- [zlib 1.2.23](https://github.com/madler/zlib)

リソース消費の大きいlibpngは使わず、シンプルに24bitRGBまたは32bitRGBAのPNGの自力デコードのみ対応しています。

また、コンパイル・アセンブル・リンクには M1 Mac上で以下のクロス開発環境を利用させて頂きました。

- [xdev68k](https://github.com/yosshin4004/xdev68k)
- [Visual Studio Code](https://code.visualstudio.com/)

なお、PNGEX.Xのコンパイルには別途 zlib をビルドして include/lib ファイルが必要です。

参考文献など

 - InsideX68000 (桑野雅彦, 1992年, ソフトバンク)
 - BMPL.X のソース (Arimac氏)

この場を借りてお礼申し上げます。

---
### 使い方

引数をつけずに実行するか、`-h` オプションをつけて実行するとヘルプメッセージが表示されます。

    PNGEX - PNG image loader with XEiJ graphic extension support version 0.1.0 by tantan 2022
    usage: pngex.x [options] <image1.png> [<image2.png> ...]
    options:
       -c ... clear graphic screen
       -e ... use extended graphic mode for XEiJ (1024x1024x65536)
       -h ... show this help message
       -i ... show file information
       -n ... image centering
       -k ... wait key input
       -v<n> ... brightness (0-100)
       -z ... show only one image randomly
       -b<n> ... buffer memory size factor[1-16] (default:8)

XEiJの拡張グラフィックモードを使って768x512画面全体に32768色の画像を表示するには必ず `-e` オプションを指定してください。

ファイル名にはワイルドカードも使用できます。

---
### 変更履歴

- version 0.3.1 (2022.12.28) ... コンパイラを gcc 12.2.0 に変更。-finput-charset=cp932 -fexec-charset=cp932 を有効化。
- version 0.2.0 (2022.12.27) ... CRTCレジスタ変更前にVSYNC待ちするようにした。スクロール位置をデフォルトにするようにした。
- version 0.1.0 (2022.12.25) ... 初版
# Unityアサーションリファレンス

## 目次

1. [背景と概要](#background-and-overview)
   1. [要点だけをまとめた説明](#super-condensed-version)
   2. [Unityには複数の役割があるが、中心はアサーション](#unity-is-several-things-but-mainly-its-assertions)
   3. [アサーションとは](#whats-an-assertion)
   4. [Unityのアサーション: 有用なメッセージと無償のソースコード文書化](#unitys-assertions-helpful-messages-and-free-source-code-documentation)
2. [アサーションの規則と設定](#assertion-conventions-and-configurations)
   1. [命名規則と引数の規則](#naming-and-parameter-conventions)
   2. [`TEST_ASSERT_EACH_EQUAL_X`派生形](#test_assert_each_equal_x-variants)
   3. [設定](#configuration)
3. [アサーション一覧](#the-assertions-in-all-their-blessed-glory)
   1. [基本的な失敗、成功、無視](#basic-fail-pass-and-ignore)
   2. [真偽値](#boolean)
   3. [各サイズの符号付き・符号なし整数](#signed-and-unsigned-integers-of-all-sizes)
   4. [各サイズの符号なし整数（16進数表示）](#unsigned-integers-of-all-sizes-in-hexadecimal)
   5. [文字](#characters)
   6. [マスクおよびビット単位のアサーション](#masked-and-bit-level-assertions)
   7. [整数の大小比較](#integer-less-than--greater-than)
   8. [各サイズの整数範囲](#integer-ranges-of-all-sizes)
   9. [構造体と文字列](#structs-and-strings)
   10. [配列](#arrays)
   11. [各サイズの整数配列範囲](#integer-array-ranges-of-all-sizes)
   12. [各要素の一致（配列と単一値の比較）](#each-equal-arrays-to-single-value)
   13. [浮動小数点数（有効な場合）](#floating-point-if-enabled)
   14. [倍精度浮動小数点数（有効な場合）](#double-if-enabled)
4. [高度なアサーション: 注意が必要なアサーションの詳細](#advanced-asserting-details-on-tricky-assertions)
   1. [`FLOAT`と`DOUBLE`の`EQUAL`アサーションはどのように動作するか](#how-do-the-equal-assertions-work-for-float-and-double)
   2. [標準的でない`int`サイズのターゲットへの対応](#how-do-we-deal-with-targets-with-non-standard-int-sizes)

<a id="background-and-overview"></a>
## 背景と概要

<a id="super-condensed-version"></a>
### 要点だけをまとめた説明

- アサーションは、1つの条件が真（真偽値の`True`）であることを確認します。
  条件が`False`の場合、アサーションは実行を停止し、失敗を報告します。
- Unityの中心は、豊富なアサーション群と、それらをまとめて簡単に実行するための仕組みです。
- Unityの構造を利用すると、テストコード内のアサーションを製品ソースコードから容易に分離できます。
- Unityのアサーションには次の特徴があります。
  - さまざまなCの型と検証ケースに対応する、多数の種類があります。
  - コンテキストを利用し、詳細で役立つ失敗メッセージを提供します。
  - ソースコード内の型、期待値、基本動作を追加コストなしで文書化します。

<a id="unity-is-several-things-but-mainly-its-assertions"></a>
### Unityには複数の役割があるが、中心はアサーション

Unityは、ソースコードが想定どおりに動作するかを確認するために利用できる、豊富なアサーション群だと考えることができます。
Unityは、製品ソースコードから分離したテストコード内で、アサーションを簡単に整理して実行するためのフレームワークを提供します。

<a id="whats-an-assertion"></a>
### アサーションとは

アサーションの本質は、真偽値として真であることの確認です。
ある値は別の値と等しいか、コードのある要素が特定の性質を持っているか、といった条件を検証します。
アサーションは実行可能なコードです。
静的解析はコード品質を改善する有益な手法ですが、アサーションのようにコードを実行するものではありません。
失敗したアサーションは実行を停止し、適切なI/O経路（標準出力、GUI、出力ファイル、LEDの点滅など）を通じてエラーを報告します。

動的な検証に最低限必要なのは、1つのアサーション機構だけです。
実際、そのためにC標準ライブラリーには[assert()マクロ][]があります。
それだけを使わない理由は、より優れた結果報告ができるからです。
Cの`assert()`はそのままでは機能が単純で、特に配列や構造体などの一般的なデータ型を扱うのが不得意です。
また、ほかの支援機能がなければ、製品ソースコードの至るところへ`assert()`を書き込みがちです。
Unityが提供する方法でテストコードと製品ソースコードを分離するほうが、通常は整理しやすく、管理しやすく、有用です。

<a id="unitys-assertions-helpful-messages-and-free-source-code-documentation"></a>
### Unityのアサーション: 有用なメッセージと無償のソースコード文書化

単純な真偽条件の検証にも価値がありますが、アサーションのコンテキストを利用すれば、さらに有益になります。
たとえば、単なる整数ではなくビットフラグを比較していると分かっているなら、その情報を利用して、失敗時に明示的で読みやすいビット単位の結果を示せます。

Unityのアサーション群はコンテキストを取得し、有用で意味のある失敗メッセージを提供します。
また、アサーション自体が、ソースコードにおける型と値についての実行可能な文書にもなります。
テストがソースコードに追従し、すべて成功している限り、ソースコードの意図と仕組みを詳細かつ最新の状態で把握できます。
不思議なことに、十分にテストされたコードは、通常よく設計されたコードにもなります。

<a id="assertion-conventions-and-configurations"></a>
## アサーションの規則と設定

<a id="naming-and-parameter-conventions"></a>
### 命名規則と引数の規則

アサーションの引数は、通常、次の順序に従います。

```c
TEST_ASSERT_X( {modifiers}, {expected}, actual, {size/count} )
```

最も単純なアサーションは、`actual`引数を1つだけ使用します（単純なNULLチェックなど）。

- `Actual`はテスト対象の値です。ほかの引数と異なり、すべてのアサーション派生形に存在する唯一の引数です。
- `Modifiers`は、マスク、範囲、ビットフラグ指定子、浮動小数点数の許容差です。
- `Expected`は`actual`と比較する期待値です。NULLチェックのように`actual`だけでよいアサーションもあるため、省略可能な引数として示されています。
- `Size/count`は、文字列長や配列要素数などを表します。

Unityには、同じデータ型を複数のアサーションで扱う、明確に類似したアサーションが多数あります。
それぞれの違いは、失敗メッセージの表示方法にあります。
たとえば、アサーションの`_HEX`派生形は、期待値と実際の値を16進数で表示します。

#### `TEST_ASSERT_X_MESSAGE`派生形

すべてのアサーションには、最後の引数として単純な文字列メッセージを受け取る派生形があります。
指定した文字列は、Unityが出力するアサーション失敗メッセージの末尾へ追加されます。

簡潔にするため、以降の一覧ではメッセージ引数を持つ派生形を記載していません。
一覧内の任意のアサーション名の末尾に`_MESSAGE`を追加し、最後の引数に文字列を渡してください。

_例:_

```c
TEST_ASSERT_X( {modifiers}, {expected}, actual, {size/count} )
```

メッセージ付きでは次のようになります。

```c
TEST_ASSERT_X_MESSAGE( {modifiers}, {expected}, actual, {size/count}, message )
```

注意:

- 多くの組み込みプロジェクトが`printf`をサポートしない、またはさまざまな理由で使用を避けるため、`_MESSAGE`派生形は意図的に`printf`形式の書式設定をサポートしていません。
  必要であれば、アサーションの前に`sprintf`を使用して複雑な失敗メッセージを組み立てられます。
- ループ内などで、アサーション失敗メッセージにカウンター値を出力したい場合は、結果の配列を作成してから後述の`_ARRAY`アサーションを使う方法が、`sprintf`の便利な代替になることがあります。

#### `TEST_ASSERT_X_ARRAY`派生形

Unityは、さまざまな型の配列に対応するアサーション群を提供します。
これらは後述の「配列」節で説明します。
Unityのほぼすべての型別アサーションは、名前に`_ARRAY`を追加することで、メモリーブロック全体を検証できます。

```c
TEST_ASSERT_EQUAL_TYPEX_ARRAY( expected, actual, {size/count} )
```

- `Expected`自体が配列です。
- `Size/count`は、配列の要素数、および必要に応じて各要素の長さを指定する1つまたは2つの引数です。

注意:

- `_MESSAGE`派生形の規則は配列アサーションにも適用されます。`_ARRAY`アサーションの`_MESSAGE`派生形は、名前が`_ARRAY_MESSAGE`で終わります。
- 浮動小数点数配列を扱うアサーションは、`float`および`double`のアサーションとまとめて記載しています。

<a id="test_assert_each_equal_x-variants"></a>
### `TEST_ASSERT_EACH_EQUAL_X`派生形

Unityは、さまざまな型の配列について、すべての要素を1つの値と比較するアサーション群も提供します。
これらは後述の「各要素の一致」節で説明します。
Unityのほぼすべての型別アサーションは、名前に`_EACH_EQUAL`を加えることで、メモリーブロック全体を検証できます。

```c
TEST_ASSERT_EACH_EQUAL_TYPEX( expected, actual, {size/count} )
```

- `Expected`は比較対象となる単一の値です。
- `Actual`は各要素を期待値と比較する配列です。
- `Size/count`は、配列の要素数、および必要に応じて各要素の長さを指定する1つまたは2つの引数です。

注意:

- `_MESSAGE`派生形の規則は、各要素を比較するアサーションにも適用されます。
- 浮動小数点数の各要素を比較するアサーションは、`float`および`double`のアサーションとまとめて記載しています。

<a id="configuration"></a>
### 設定

#### 浮動小数点数のサポートは任意

浮動小数点型のサポートは設定可能です。
適切なプリプロセッサーシンボルを定義することで、Unityコード内の`float`と`double`を個別に有効化または無効化できます。
これは、浮動小数点演算をサポートしない組み込みターゲットで有用です。固定小数点数だけのプラットフォームでもUnityをエラーなくコンパイルできます。
詳細はUnityの文書を参照してください。

#### データ型の最大幅は設定可能

すべてのターゲットが64ビット型や32ビット型をサポートしているわけではありません。
適切なプリプロセッサーシンボルを定義すると、Unityはターゲットの最大幅を超えるすべての処理をコンパイル対象から除外します。
詳細はUnityの文書を参照してください。

<a id="the-assertions-in-all-their-blessed-glory"></a>
## アサーション一覧

<a id="basic-fail-pass-and-ignore"></a>
### 基本的な失敗、成功、無視

#### `TEST_FAIL()`

#### `TEST_FAIL_MESSAGE("message")`

主に、テストコードが単純なアサーションを超えるロジックを実行する特殊な状況で使用します。
実際には、`TEST_FAIL()`は常に条件分岐ブロック内で使われます。

_例:_

- カウンターを増加させるステートマシンを複数回実行し、最後にテストコードでカウンターを検証する。
- 例外を発生させて検証する（Try / Catch / Throwなど。[CException](https://github.com/ThrowTheSwitch/CException)プロジェクトを参照）。

#### `TEST_PASS()`

#### `TEST_PASS_MESSAGE("message")`

テストの残りを中断しますが、そのテストは成功として集計します。
通常、このマクロをテストに含める必要はありません。失敗しなければ、自動的に`PASS`として集計されます。
`#ifdef`などを含むテストで役立つ場合があります。

#### `TEST_IGNORE()`

#### `TEST_IGNORE_MESSAGE("message")`

テストケース、つまりテストアサーションを含む関数を無視対象としてマークします。
通常は、後で戻ってテストケースを実装するための目印として使用します。
同じテストケース内にほかのアサーションがある場合、テストの無視が及ぼす影響に注意が必要です。詳細はUnityの文書を参照してください。

#### `TEST_MESSAGE(message)`

テストを終了せずに、Unityの出力ストリームへ`INFO`メッセージを出力するときに便利です。
成功・失敗メッセージと同様に、ファイル名および行番号とともに出力されます。

<a id="boolean"></a>
### 真偽値

#### `TEST_ASSERT(condition)`

#### `TEST_ASSERT_TRUE(condition)`

#### `TEST_ASSERT_FALSE(condition)`

#### `TEST_ASSERT_UNLESS(condition)`

`TEST_ASSERT_FALSE`の表現を変えたものです。
`TEST_ASSERT_UNLESS`という名前により、特定のテスト構造や条件文で読みやすくなる場合があります。

#### `TEST_ASSERT_NULL(pointer)`

#### `TEST_ASSERT_NOT_NULL(pointer)`

ポインターがNULLである、またはNULLでないことを検証します。

#### `TEST_ASSERT_EMPTY(pointer)`

#### `TEST_ASSERT_NOT_EMPTY(pointer)`

ポインターをデリファレンスした最初の要素が0である、または0でないことを検証します。
特に、NULL終端C文字列が空かどうかの確認に便利ですが、ほかのNULL終端配列にも使用できます。

<a id="signed-and-unsigned-integers-of-all-sizes"></a>
### 各サイズの符号付き・符号なし整数

大きな整数サイズをサポートしないビルドターゲットでは、それらを無効化できます。
たとえば、ターゲットが最大16ビット型までしかサポートしない場合、適切なシンボルを定義することで、コンパイルを失敗させる32ビットおよび64ビット処理をUnityから除外できます。
ほかのワードサイズへの対応については、この文書の「高度なアサーション」を参照してください。

#### `TEST_ASSERT_EQUAL_INT(expected, actual)`

#### `TEST_ASSERT_EQUAL_INT8(expected, actual)`

#### `TEST_ASSERT_EQUAL_INT16(expected, actual)`

#### `TEST_ASSERT_EQUAL_INT32(expected, actual)`

#### `TEST_ASSERT_EQUAL_INT64(expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT(expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT8(expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT16(expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT32(expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT64(expected, actual)`

<a id="unsigned-integers-of-all-sizes-in-hexadecimal"></a>
### 各サイズの符号なし整数（16進数表示）

すべての`_HEX`アサーションは、機能的には符号なし整数アサーションと同じですが、失敗メッセージ内の`expected`と`actual`を16進数で表示します。
Unityの出力はビッグエンディアンです。

#### `TEST_ASSERT_EQUAL_HEX(expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX8(expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX16(expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX32(expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX64(expected, actual)`

<a id="characters"></a>
### 文字

`char`の比較には8ビット整数アサーションも使用できますが、専用の次のアサーションも使用できます。
印字可能文字はそのまま表示し、それ以外の文字は16進数のエスケープコードで表示します。

#### `TEST_ASSERT_EQUAL_CHAR(expected, actual)`

<a id="masked-and-bit-level-assertions"></a>
### マスクおよびビット単位のアサーション

マスクおよびビット単位のアサーションは、結果を16進数で出力します。
Unityの出力はビッグエンディアンです。

#### `TEST_ASSERT_BITS(mask, expected, actual)`

`expected`と`actual`のうち、マスクされたビットだけを比較します。

#### `TEST_ASSERT_BITS_HIGH(mask, actual)`

`actual`のマスクされたビットがHIGHであることを検証します。

#### `TEST_ASSERT_BITS_LOW(mask, actual)`

`actual`のマスクされたビットがLOWであることを検証します。

#### `TEST_ASSERT_BIT_HIGH(bit, actual)`

`actual`の指定ビットがHIGHであることを検証します。

#### `TEST_ASSERT_BIT_LOW(bit, actual)`

`actual`の指定ビットがLOWであることを検証します。

<a id="integer-less-than--greater-than"></a>
### 整数の大小比較

これらのアサーションは、`actual`が`threshold`より小さい、または大きいことを検証します。しきい値自体は範囲に含まれません。
たとえば、より大きいことを検証するアサーションでしきい値が0なら、値が0以下の場合に失敗します。
等価比較アサーションと同様、さまざまな整数サイズに対応するものがあります。

#### `TEST_ASSERT_GREATER_THAN_INT8(threshold, actual)`

#### `TEST_ASSERT_GREATER_OR_EQUAL_INT16(threshold, actual)`

#### `TEST_ASSERT_LESS_THAN_INT32(threshold, actual)`

#### `TEST_ASSERT_LESS_OR_EQUAL_UINT(threshold, actual)`

#### `TEST_ASSERT_NOT_EQUAL_UINT8(threshold, actual)`

<a id="integer-ranges-of-all-sizes"></a>
### 各サイズの整数範囲

これらのアサーションは、`actual`が`expected`のプラスマイナス`delta`以内（境界を含む）であることを検証します。
たとえば、期待値が10、許容差が3の場合、7から13の範囲外の値で失敗します。

#### `TEST_ASSERT_INT_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_INT8_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_INT16_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_INT32_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_INT64_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_UINT_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_UINT8_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_UINT16_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_UINT32_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_UINT64_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_HEX_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_HEX8_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_HEX16_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_HEX32_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_HEX64_WITHIN(delta, expected, actual)`

#### `TEST_ASSERT_CHAR_WITHIN(delta, expected, actual)`

<a id="structs-and-strings"></a>
### 構造体と文字列

#### `TEST_ASSERT_EQUAL_PTR(expected, actual)`

ポインターが同じメモリー位置を指していることを検証します。

#### `TEST_ASSERT_EQUAL_STRING(expected, actual)`

NULL文字（`'\0'`）で終端された文字列が同一であることを検証します。
文字列の長さが異なる場合や、終端文字より前の一部でも異なる場合は失敗します。
2つのNULL文字列、つまり長さ0の文字列は等しいと見なされます。

#### `TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)`

`expected`と`actual`のポインターが指すメモリーの内容が同一であることを検証します。
メモリーブロックのバイト数は`len`で指定します。

<a id="arrays"></a>
### 配列

`expected`と`actual`はどちらも配列です。
`num_elements`は比較する配列要素数を指定します。

`_HEX`アサーションは、期待値と実際の配列内容を16進数で失敗メッセージへ表示します。

文字列配列の比較動作については、前節の`TEST_ASSERT_EQUAL_STRING`の説明を参照してください。

比較する配列内で最初に不一致の要素が見つかった時点で失敗します。
失敗メッセージには、比較に失敗した配列インデックスが示されます。

#### `TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT8_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT16_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT32_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT64_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT64_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX64_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_CHAR_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_PTR_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, len, num_elements)`

`len`は、配列の各要素で比較するメモリーのバイト数です。

<a id="integer-array-ranges-of-all-sizes"></a>
### 各サイズの整数配列範囲

これらのアサーションは、`actual`配列の各要素が`expected`配列の対応する値のプラスマイナス`delta`以内（境界を含む）であることを検証します。
たとえば、期待値が\[10, 12\]、許容差が3の場合、\[7 - 13, 9 - 15\]の各範囲外の値で失敗します。

#### `TEST_ASSERT_INT_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT8_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT16_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT32_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT64_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT8_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT16_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT32_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT64_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX8_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX16_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX32_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX64_ARRAY_WITHIN(delta, expected, actual, num_elements)`

#### `TEST_ASSERT_CHAR_ARRAY_WITHIN(delta, expected, actual, num_elements)`

<a id="each-equal-arrays-to-single-value"></a>
### 各要素の一致（配列と単一値の比較）

`expected`は単一の値、`actual`は配列です。
`num_elements`は比較する配列要素数を指定します。

`_HEX`アサーションは、期待値と実際の配列内容を16進数で失敗メッセージへ表示します。

配列内で最初に不一致の要素が見つかった時点で失敗します。
失敗メッセージには、比較に失敗した配列インデックスが示されます。

#### `TEST_ASSERT_EACH_EQUAL_INT(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT8(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT16(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT32(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT64(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT8(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT16(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT32(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT64(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX8(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX16(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX32(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX64(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_CHAR(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_PTR(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_STRING(expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_MEMORY(expected, actual, len, num_elements)`

`len`は、配列の各要素で比較するメモリーのバイト数です。

<a id="floating-point-if-enabled"></a>
### 浮動小数点数（有効な場合）

#### `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)`

`actual`が`expected`のプラスマイナス`delta`以内であることを検証します。
浮動小数点数の表現上、厳密な等価性の評価は保証されません。

#### `TEST_ASSERT_FLOAT_NOT_WITHIN(delta, expected, actual)`

`actual`が`expected`のプラスマイナス`delta`以内ではないことを検証します。

#### `TEST_ASSERT_EQUAL_FLOAT(expected, actual)`

`actual`が`expected`と「等しいと見なせる程度に近い」ことを検証します。
詳細は「高度なアサーション」を参照してください。
浮動小数点数アサーションでユーザー指定の許容差を省略できることは、簡略記法として便利であるだけでなく、CMockのコード生成規則においても必要です。

#### `TEST_ASSERT_NOT_EQUAL_FLOAT(expected, actual)`

`actual`が`expected`と「等しいと見なせる程度に近くない」ことを検証します。

#### `TEST_ASSERT_FLOAT_ARRAY_WITHIN(delta, expected, actual, num_elements)`

詳細は「配列」節を参照してください。
配列の各要素は、ユーザー指定の許容差と既定の比較許容差を加えた値で検証され、`TEST_ASSERT_FLOAT_WITHIN`の比較に基づきます。

#### `TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected, actual, num_elements)`

詳細は「配列」節を参照してください。
配列の各浮動小数点要素は`TEST_ASSERT_EQUAL_FLOAT`を使用して比較されます。
ユーザー指定の許容差を使うには、浮動小数点配列用アサーションを独自に実装する必要があります。

#### `TEST_ASSERT_LESS_THAN_FLOAT(threshold, actual)`

`actual`が`threshold`より小さいことを検証します。しきい値自体は含みません。
たとえば、しきい値が`1.0f`の場合、値が`1.0f`以上なら失敗します。

#### `TEST_ASSERT_GREATER_THAN_FLOAT(threshold, actual)`

`actual`が`threshold`より大きいことを検証します。しきい値自体は含みません。
たとえば、しきい値が`1.0f`の場合、値が`1.0f`以下なら失敗します。

#### `TEST_ASSERT_LESS_OR_EQUAL_FLOAT(threshold, actual)`

`actual`が`threshold`以下であることを検証します。
等価性の規則は`TEST_ASSERT_EQUAL_FLOAT`と同じです。

#### `TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(threshold, actual)`

`actual`が`threshold`以上であることを検証します。
等価性の規則は`TEST_ASSERT_EQUAL_FLOAT`と同じです。

#### `TEST_ASSERT_FLOAT_IS_INF(actual)`

`actual`が正の無限大を表す浮動小数点表現と等しいことを検証します。

#### `TEST_ASSERT_FLOAT_IS_NEG_INF(actual)`

`actual`が負の無限大を表す浮動小数点表現と等しいことを検証します。

#### `TEST_ASSERT_FLOAT_IS_NAN(actual)`

`actual`が非数（NaN）の浮動小数点表現であることを検証します。

#### `TEST_ASSERT_FLOAT_IS_DETERMINATE(actual)`

`actual`が数学演算に使用可能な浮動小数点表現であることを検証します。
つまり、正の無限大、負の無限大、非数のいずれでもないことを確認します。

#### `TEST_ASSERT_FLOAT_IS_NOT_INF(actual)`

`actual`が正の無限大以外の値であることを検証します。

#### `TEST_ASSERT_FLOAT_IS_NOT_NEG_INF(actual)`

`actual`が負の無限大以外の値であることを検証します。

#### `TEST_ASSERT_FLOAT_IS_NOT_NAN(actual)`

`actual`が非数以外の値であることを検証します。

#### `TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE(actual)`

`actual`が数学演算に使用できないことを検証します。
つまり、正の無限大、負の無限大、非数のいずれかであることを確認します。

<a id="double-if-enabled"></a>
### 倍精度浮動小数点数（有効な場合）

#### `TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)`

`actual`が`expected`のプラスマイナス`delta`以内であることを検証します。
浮動小数点数の表現上、厳密な等価性の評価は保証されません。

#### `TEST_ASSERT_DOUBLE_NOT_WITHIN(delta, expected, actual)`

`actual`が`expected`のプラスマイナス`delta`以内ではないことを検証します。

#### `TEST_ASSERT_EQUAL_DOUBLE(expected, actual)`

`actual`が`expected`と「等しいと見なせる程度に近い」ことを検証します。
詳細は「高度なアサーション」を参照してください。
浮動小数点数アサーションでユーザー指定の許容差を省略できることは、簡略記法として便利であるだけでなく、CMockのコード生成規則においても必要です。

#### `TEST_ASSERT_NOT_EQUAL_DOUBLE(expected, actual)`

`actual`が`expected`と「等しいと見なせる程度に近くない」ことを検証します。

#### `TEST_ASSERT_DOUBLE_ARRAY_WITHIN(delta, expected, actual, num_elements)`

詳細は「配列」節を参照してください。
配列の各要素は、ユーザー指定の許容差と既定の比較許容差を加えた値で検証され、`TEST_ASSERT_DOUBLE_WITHIN`の比較に基づきます。

#### `TEST_ASSERT_EQUAL_DOUBLE_ARRAY(expected, actual, num_elements)`

詳細は「配列」節を参照してください。
配列の各倍精度浮動小数点要素は`TEST_ASSERT_EQUAL_DOUBLE`を使用して比較されます。
ユーザー指定の許容差を使うには、倍精度浮動小数点配列用アサーションを独自に実装する必要があります。

#### `TEST_ASSERT_LESS_THAN_DOUBLE(threshold, actual)`

`actual`が`threshold`より小さいことを検証します。しきい値自体は含みません。
たとえば、しきい値が`1.0`の場合、値が`1.0`以上なら失敗します。

#### `TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(threshold, actual)`

`actual`が`threshold`以下であることを検証します。
等価性の規則は`TEST_ASSERT_EQUAL_DOUBLE`と同じです。

#### `TEST_ASSERT_GREATER_THAN_DOUBLE(threshold, actual)`

`actual`が`threshold`より大きいことを検証します。しきい値自体は含みません。
たとえば、しきい値が`1.0`の場合、値が`1.0`以下なら失敗します。

#### `TEST_ASSERT_GREATER_OR_EQUAL_DOUBLE(threshold, actual)`

`actual`が`threshold`以上であることを検証します。
等価性の規則は`TEST_ASSERT_EQUAL_DOUBLE`と同じです。

#### `TEST_ASSERT_DOUBLE_IS_INF(actual)`

`actual`が正の無限大を表す浮動小数点表現と等しいことを検証します。

#### `TEST_ASSERT_DOUBLE_IS_NEG_INF(actual)`

`actual`が負の無限大を表す浮動小数点表現と等しいことを検証します。

#### `TEST_ASSERT_DOUBLE_IS_NAN(actual)`

`actual`が非数（NaN）の浮動小数点表現であることを検証します。

#### `TEST_ASSERT_DOUBLE_IS_DETERMINATE(actual)`

`actual`が数学演算に使用可能な浮動小数点表現であることを検証します。
つまり、正の無限大、負の無限大、非数のいずれでもないことを確認します。

#### `TEST_ASSERT_DOUBLE_IS_NOT_INF(actual)`

`actual`が正の無限大以外の値であることを検証します。

#### `TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF(actual)`

`actual`が負の無限大以外の値であることを検証します。

#### `TEST_ASSERT_DOUBLE_IS_NOT_NAN(actual)`

`actual`が非数以外の値であることを検証します。

#### `TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE(actual)`

`actual`が数学演算に使用できないことを検証します。
つまり、正の無限大、負の無限大、非数のいずれかであることを確認します。

<a id="advanced-asserting-details-on-tricky-assertions"></a>
## 高度なアサーション: 注意が必要なアサーションの詳細

この節では、遭遇する可能性がある注意の必要なアサーションについて説明します。
Unityのアサーション機構が内部でどのように動作するかも少し紹介します。
背後の仕組みを知りたい場合は、このまま読み進めてください。
必要になるまで詳細を知る必要がなければ、ここから先を読み飛ばしても構いません。

<a id="how-do-the-equal-assertions-work-for-float-and-double"></a>
### `FLOAT`と`DOUBLE`の`EQUAL`アサーションはどのように動作するか

2つの`float`値や`double`値を直接比較して等価性を確認することは、精度に問題があり、避けるべき場合があります。
浮動小数点値は、特に一連の演算を行った後では、複数の方法で表現されることがあります。
変数を2.0で初期化すると`2 x 2^0`という浮動小数点表現になる可能性がありますが、一連の数学演算を行った結果、同じ値2を表す`8 x 2^-2`になる場合もあります。
演算を繰り返すうちに、等価比較が失敗することがあります。

そのため、Unityは浮動小数点数の等価性を直接比較しません。
代わりに、2つの浮動小数点値が「十分に近い」かを確認します。
Unityを既定設定のまま使用する場合、「十分に近い」とは「有効ビット1つか2つ以内」を意味します。
内部的に`TEST_ASSERT_EQUAL_FLOAT`は、`delta`をその場で計算する`TEST_ASSERT_FLOAT_WITHIN`として動作します。
単精度では、期待値に0.00001を掛けて許容差を求め、期待値の周囲に非常に小さい比例範囲を作ります。

期待値が20,000.0の場合、許容差は0.2になります。
したがって、19,999.8から20,000.2までの値が等価性チェックを満たします。
これは単精度数の約1ビット分の範囲であり、浮動小数点値に対して合理的に設定できる、ほぼ最も厳しい許容範囲です。

では、値が0の場合はどうなるでしょうか。
0はほかの浮動小数点値以上に、さまざまな方法で表現できます。
0x20でも0x263でも、値としては0です。
これらの値を互いに減算すれば、差は常に0となり、許容差0のプラスマイナス範囲にも収まるため、正しく動作します。

倍精度浮動小数点数では、さらに小さい乗数を使用し、同様に約1ビット分の誤差へ近似します。

この範囲が適さず、浮動小数点数の等価性アサーションを緩くしたい場合は、`UNITY_FLOAT_PRECISION`および`UNITY_DOUBLE_PRECISION`を定義して乗数を任意の値へ変更できます。
詳細はUnityの文書を参照してください。

<a id="how-do-we-deal-with-targets-with-non-standard-int-sizes"></a>
### 標準的でない`int`サイズのターゲットへの対応

Cでは、整数のような基本的な型のサイズもターゲットによって異なります。
C標準では、`int`はターゲットの自然なレジスターサイズで、少なくとも16ビットかつ1バイトの整数倍であることが求められます。
また、型サイズの順序も保証されます。

```C
char <= short <= int <= long <= long long
```

多くの場合、`int`は32ビットです。
組み込み分野では、`int`が16ビットの場合も多数あります。
24ビット整数を持つ珍しいマイクロコントローラーも存在し、これも標準Cの範囲内です。

さらに、難しい選択を迫られるコンパイラーやターゲットもあります。
自然なレジスターサイズが10ビットや12ビットの場合、16ビット以上という要件と自然なレジスターサイズに一致するという要件を同時には満たせません。
このような場合、自然なレジスターサイズを選択し、次のような構成になることがあります。

```C
char (8ビット) <= short (12ビット) <= int (12ビット) <= long (16ビット)
```

これは明らかに規則の一部に違反していますが、いずれかの規則を破る必要があるため、そのような選択が行われています。

C99標準では、固定幅の代替整数型が導入されました。
整数型の最小値と最大値を取得するマクロも導入されています。
これは非常に便利ですが、多くの組み込みコンパイラーがC99の型を使用できるとは限りません。
前述のような特殊なレジスターサイズが理由の場合もあれば、単に対応していない場合もあります。

Unityは当初から、マイクロコントローラーまたはマイクロプロセッサーとCコンパイラーのあらゆる組み合わせをサポートすることを目標としてきました。
長い時間をかけて、その目標へかなり近づいています。
ただし、特徴の強いターゲットで効果的に利用するには、いくつか知っておくべき方法があります。

まず、新しいターゲット用にUnityを設定する際は、型を自動検出するマクロ（利用できる場合）または手動設定用マクロに特に注意してください。
どちらについてもUnityの文書で確認できます。

24ビット`int`のような特殊な型を扱う必要がある場合、最も簡単な解決策は1段階大きいサイズを使うことです。
24ビット`int`ならUnityを32ビット整数に、12ビット`int`なら16ビット整数に設定します。
これには次の2つの影響があります。

1. Unityがエラーを表示するとき、未使用の上位ビットが0で埋められます。
2. 符号付き演算を行うアサーション、特に`TEST_ASSERT_INT_WITHIN`では注意が必要です。
   `int`が誤った位置でラップアラウンドし、誤検出が起きる可能性があります。
   単純な`TEST_ASSERT`へ戻し、演算を自分で行うこともできます。

*最新情報や詳細は[ThrowTheSwitch.org][]を参照してください。*

[assert()マクロ]: http://en.wikipedia.org/wiki/Assert.h
[ThrowTheSwitch.org]: https://throwtheswitch.org

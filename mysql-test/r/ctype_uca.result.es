DROP TABLE IF EXISTS t1;
set names utf8;
set collation_connection=utf8_unicode_ci;
select 'a' = 'a', 'a' = 'a ', 'a ' = 'a';
'a' = 'a'	'a' = 'a '	'a ' = 'a'
1	1	1
select 'a\t' = 'a' , 'a\t' < 'a' , 'a\t' > 'a';
'a\t' = 'a'	'a\t' < 'a'	'a\t' > 'a'
0	1	0
select 'a\t' = 'a ', 'a\t' < 'a ', 'a\t' > 'a ';
'a\t' = 'a '	'a\t' < 'a '	'a\t' > 'a '
0	1	0
select 'a' = 'a\t', 'a' < 'a\t', 'a' > 'a\t';
'a' = 'a\t'	'a' < 'a\t'	'a' > 'a\t'
0	0	1
select 'a ' = 'a\t', 'a ' < 'a\t', 'a ' > 'a\t';
'a ' = 'a\t'	'a ' < 'a\t'	'a ' > 'a\t'
0	0	1
select 'a  a' > 'a', 'a  \t' < 'a';
'a  a' > 'a'	'a  \t' < 'a'
1	1
select 'c' like '\_' as want0;
want0
0
CREATE TABLE t (
c char(20) NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
INSERT INTO t VALUES ('a'),('ab'),('aba');
ALTER TABLE t ADD INDEX (c);
SELECT c FROM t WHERE c LIKE 'a%';
c
a
ab
aba
DROP TABLE t;
create table t1 (c1 char(10) character set utf8 collate utf8_bin);
insert into t1 values ('A'),('a');
insert into t1 values ('B'),('b');
insert into t1 values ('C'),('c');
insert into t1 values ('D'),('d');
insert into t1 values ('E'),('e');
insert into t1 values ('F'),('f');
insert into t1 values ('G'),('g');
insert into t1 values ('H'),('h');
insert into t1 values ('I'),('i');
insert into t1 values ('J'),('j');
insert into t1 values ('K'),('k');
insert into t1 values ('L'),('l');
insert into t1 values ('M'),('m');
insert into t1 values ('N'),('n');
insert into t1 values ('O'),('o');
insert into t1 values ('P'),('p');
insert into t1 values ('Q'),('q');
insert into t1 values ('R'),('r');
insert into t1 values ('S'),('s');
insert into t1 values ('T'),('t');
insert into t1 values ('U'),('u');
insert into t1 values ('V'),('v');
insert into t1 values ('W'),('w');
insert into t1 values ('X'),('x');
insert into t1 values ('Y'),('y');
insert into t1 values ('Z'),('z');
insert into t1 values (_ucs2 0x00e0),(_ucs2 0x00c0);
insert into t1 values (_ucs2 0x00e1),(_ucs2 0x00c1);
insert into t1 values (_ucs2 0x00e2),(_ucs2 0x00c2);
insert into t1 values (_ucs2 0x00e3),(_ucs2 0x00c3);
insert into t1 values (_ucs2 0x00e4),(_ucs2 0x00c4);
insert into t1 values (_ucs2 0x00e5),(_ucs2 0x00c5);
insert into t1 values (_ucs2 0x00e6),(_ucs2 0x00c6);
insert into t1 values (_ucs2 0x00e7),(_ucs2 0x00c7);
insert into t1 values (_ucs2 0x00e8),(_ucs2 0x00c8);
insert into t1 values (_ucs2 0x00e9),(_ucs2 0x00c9);
insert into t1 values (_ucs2 0x00ea),(_ucs2 0x00ca);
insert into t1 values (_ucs2 0x00eb),(_ucs2 0x00cb);
insert into t1 values (_ucs2 0x00ec),(_ucs2 0x00cc);
insert into t1 values (_ucs2 0x00ed),(_ucs2 0x00cd);
insert into t1 values (_ucs2 0x00ee),(_ucs2 0x00ce);
insert into t1 values (_ucs2 0x00ef),(_ucs2 0x00cf);
insert into t1 values (_ucs2 0x00f0),(_ucs2 0x00d0);
insert into t1 values (_ucs2 0x00f1),(_ucs2 0x00d1);
insert into t1 values (_ucs2 0x00f2),(_ucs2 0x00d2);
insert into t1 values (_ucs2 0x00f3),(_ucs2 0x00d3);
insert into t1 values (_ucs2 0x00f4),(_ucs2 0x00d4);
insert into t1 values (_ucs2 0x00f5),(_ucs2 0x00d5);
insert into t1 values (_ucs2 0x00f6),(_ucs2 0x00d6);
insert into t1 values (_ucs2 0x00f7),(_ucs2 0x00d7);
insert into t1 values (_ucs2 0x00f8),(_ucs2 0x00d8);
insert into t1 values (_ucs2 0x00f9),(_ucs2 0x00d9);
insert into t1 values (_ucs2 0x00fa),(_ucs2 0x00da);
insert into t1 values (_ucs2 0x00fb),(_ucs2 0x00db);
insert into t1 values (_ucs2 0x00fc),(_ucs2 0x00dc);
insert into t1 values (_ucs2 0x00fd),(_ucs2 0x00dd);
insert into t1 values (_ucs2 0x00fe),(_ucs2 0x00de);
insert into t1 values (_ucs2 0x00ff),(_ucs2 0x00df);
insert into t1 values (_ucs2 0x0100),(_ucs2 0x0101),(_ucs2 0x0102),(_ucs2 0x0103);
insert into t1 values (_ucs2 0x0104),(_ucs2 0x0105),(_ucs2 0x0106),(_ucs2 0x0107);
insert into t1 values (_ucs2 0x0108),(_ucs2 0x0109),(_ucs2 0x010a),(_ucs2 0x010b);
insert into t1 values (_ucs2 0x010c),(_ucs2 0x010d),(_ucs2 0x010e),(_ucs2 0x010f);
insert into t1 values (_ucs2 0x0110),(_ucs2 0x0111),(_ucs2 0x0112),(_ucs2 0x0113);
insert into t1 values (_ucs2 0x0114),(_ucs2 0x0115),(_ucs2 0x0116),(_ucs2 0x0117);
insert into t1 values (_ucs2 0x0118),(_ucs2 0x0119),(_ucs2 0x011a),(_ucs2 0x011b);
insert into t1 values (_ucs2 0x011c),(_ucs2 0x011d),(_ucs2 0x011e),(_ucs2 0x011f);
insert into t1 values (_ucs2 0x0120),(_ucs2 0x0121),(_ucs2 0x0122),(_ucs2 0x0123);
insert into t1 values (_ucs2 0x0124),(_ucs2 0x0125),(_ucs2 0x0126),(_ucs2 0x0127);
insert into t1 values (_ucs2 0x0128),(_ucs2 0x0129),(_ucs2 0x012a),(_ucs2 0x012b);
insert into t1 values (_ucs2 0x012c),(_ucs2 0x012d),(_ucs2 0x012e),(_ucs2 0x012f);
insert into t1 values (_ucs2 0x0130),(_ucs2 0x0131),(_ucs2 0x0132),(_ucs2 0x0133);
insert into t1 values (_ucs2 0x0134),(_ucs2 0x0135),(_ucs2 0x0136),(_ucs2 0x0137);
insert into t1 values (_ucs2 0x0138),(_ucs2 0x0139),(_ucs2 0x013a),(_ucs2 0x013b);
insert into t1 values (_ucs2 0x013c),(_ucs2 0x013d),(_ucs2 0x013e),(_ucs2 0x013f);
insert into t1 values (_ucs2 0x0140),(_ucs2 0x0141),(_ucs2 0x0142),(_ucs2 0x0143);
insert into t1 values (_ucs2 0x0144),(_ucs2 0x0145),(_ucs2 0x0146),(_ucs2 0x0147);
insert into t1 values (_ucs2 0x0148),(_ucs2 0x0149),(_ucs2 0x014a),(_ucs2 0x014b);
insert into t1 values (_ucs2 0x014c),(_ucs2 0x014d),(_ucs2 0x014e),(_ucs2 0x014f);
insert into t1 values (_ucs2 0x0150),(_ucs2 0x0151),(_ucs2 0x0152),(_ucs2 0x0153);
insert into t1 values (_ucs2 0x0154),(_ucs2 0x0155),(_ucs2 0x0156),(_ucs2 0x0157);
insert into t1 values (_ucs2 0x0158),(_ucs2 0x0159),(_ucs2 0x015a),(_ucs2 0x015b);
insert into t1 values (_ucs2 0x015c),(_ucs2 0x015d),(_ucs2 0x015e),(_ucs2 0x015f);
insert into t1 values (_ucs2 0x0160),(_ucs2 0x0161),(_ucs2 0x0162),(_ucs2 0x0163);
insert into t1 values (_ucs2 0x0164),(_ucs2 0x0165),(_ucs2 0x0166),(_ucs2 0x0167);
insert into t1 values (_ucs2 0x0168),(_ucs2 0x0169),(_ucs2 0x016a),(_ucs2 0x016b);
insert into t1 values (_ucs2 0x016c),(_ucs2 0x016d),(_ucs2 0x016e),(_ucs2 0x016f);
insert into t1 values (_ucs2 0x0170),(_ucs2 0x0171),(_ucs2 0x0172),(_ucs2 0x0173);
insert into t1 values (_ucs2 0x0174),(_ucs2 0x0175),(_ucs2 0x0176),(_ucs2 0x0177);
insert into t1 values (_ucs2 0x0178),(_ucs2 0x0179),(_ucs2 0x017a),(_ucs2 0x017b);
insert into t1 values (_ucs2 0x017c),(_ucs2 0x017d),(_ucs2 0x017e),(_ucs2 0x017f);
insert into t1 values (_ucs2 0x0180),(_ucs2 0x0181),(_ucs2 0x0182),(_ucs2 0x0183);
insert into t1 values (_ucs2 0x0184),(_ucs2 0x0185),(_ucs2 0x0186),(_ucs2 0x0187);
insert into t1 values (_ucs2 0x0188),(_ucs2 0x0189),(_ucs2 0x018a),(_ucs2 0x018b);
insert into t1 values (_ucs2 0x018c),(_ucs2 0x018d),(_ucs2 0x018e),(_ucs2 0x018f);
insert into t1 values (_ucs2 0x0190),(_ucs2 0x0191),(_ucs2 0x0192),(_ucs2 0x0193);
insert into t1 values (_ucs2 0x0194),(_ucs2 0x0195),(_ucs2 0x0196),(_ucs2 0x0197);
insert into t1 values (_ucs2 0x0198),(_ucs2 0x0199),(_ucs2 0x019a),(_ucs2 0x019b);
insert into t1 values (_ucs2 0x019c),(_ucs2 0x019d),(_ucs2 0x019e),(_ucs2 0x019f);
insert into t1 values (_ucs2 0x01a0),(_ucs2 0x01a1),(_ucs2 0x01a2),(_ucs2 0x01a3);
insert into t1 values (_ucs2 0x01a4),(_ucs2 0x01a5),(_ucs2 0x01a6),(_ucs2 0x01a7);
insert into t1 values (_ucs2 0x01a8),(_ucs2 0x01a9),(_ucs2 0x01aa),(_ucs2 0x01ab);
insert into t1 values (_ucs2 0x01ac),(_ucs2 0x01ad),(_ucs2 0x01ae),(_ucs2 0x01af);
insert into t1 values (_ucs2 0x01b0),(_ucs2 0x01b1),(_ucs2 0x01b2),(_ucs2 0x01b3);
insert into t1 values (_ucs2 0x01b4),(_ucs2 0x01b5),(_ucs2 0x01b6),(_ucs2 0x01b7);
insert into t1 values (_ucs2 0x01b8),(_ucs2 0x01b9),(_ucs2 0x01ba),(_ucs2 0x01bb);
insert into t1 values (_ucs2 0x01bc),(_ucs2 0x01bd),(_ucs2 0x01be),(_ucs2 0x01bf);
insert into t1 values (_ucs2 0x01c0),(_ucs2 0x01c1),(_ucs2 0x01c2),(_ucs2 0x01c3);
insert into t1 values (_ucs2 0x01c4),(_ucs2 0x01c5),(_ucs2 0x01c6),(_ucs2 0x01c7);
insert into t1 values (_ucs2 0x01c8),(_ucs2 0x01c9),(_ucs2 0x01ca),(_ucs2 0x01cb);
insert into t1 values (_ucs2 0x01cc),(_ucs2 0x01cd),(_ucs2 0x01ce),(_ucs2 0x01cf);
insert into t1 values (_ucs2 0x01d0),(_ucs2 0x01d1),(_ucs2 0x01d2),(_ucs2 0x01d3);
insert into t1 values (_ucs2 0x01d4),(_ucs2 0x01d5),(_ucs2 0x01d6),(_ucs2 0x01d7);
insert into t1 values (_ucs2 0x01d8),(_ucs2 0x01d9),(_ucs2 0x01da),(_ucs2 0x01db);
insert into t1 values (_ucs2 0x01dc),(_ucs2 0x01dd),(_ucs2 0x01de),(_ucs2 0x01df);
insert into t1 values (_ucs2 0x01e0),(_ucs2 0x01e1),(_ucs2 0x01e2),(_ucs2 0x01e3);
insert into t1 values (_ucs2 0x01e4),(_ucs2 0x01e5),(_ucs2 0x01e6),(_ucs2 0x01e7);
insert into t1 values (_ucs2 0x01e8),(_ucs2 0x01e9),(_ucs2 0x01ea),(_ucs2 0x01eb);
insert into t1 values (_ucs2 0x01ec),(_ucs2 0x01ed),(_ucs2 0x01ee),(_ucs2 0x01ef);
insert into t1 values (_ucs2 0x01f0),(_ucs2 0x01f1),(_ucs2 0x01f2),(_ucs2 0x01f3);
insert into t1 values (_ucs2 0x01f4),(_ucs2 0x01f5),(_ucs2 0x01f6),(_ucs2 0x01f7);
insert into t1 values (_ucs2 0x01f8),(_ucs2 0x01f9),(_ucs2 0x01fa),(_ucs2 0x01fb);
insert into t1 values (_ucs2 0x01fc),(_ucs2 0x01fd),(_ucs2 0x01fe),(_ucs2 0x01ff);
insert into t1 values ('AA'),('Aa'),('aa'),('aA');
insert into t1 values ('CH'),('Ch'),('ch'),('cH');
insert into t1 values ('DZ'),('Dz'),('dz'),('dZ');
insert into t1 values ('IJ'),('Ij'),('ij'),('iJ');
insert into t1 values ('LJ'),('Lj'),('lj'),('lJ');
insert into t1 values ('LL'),('Ll'),('ll'),('lL');
insert into t1 values ('NJ'),('Nj'),('nj'),('nJ');
insert into t1 values ('OE'),('Oe'),('oe'),('oE');
insert into t1 values ('SS'),('Ss'),('ss'),('sS');
insert into t1 values ('RR'),('Rr'),('rr'),('rR');
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_unicode_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_icelandic_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Â,Ã,à,â,ã,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Á,á
Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Ð,ð
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
E,e,È,Ê,Ë,è,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
É,é
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Î,Ï,ì,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
Í,í
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ô,Õ,ò,ô,õ,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ó,ó
Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Û,Ü,ù,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ú,ú
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,ÿ,Ŷ,ŷ,Ÿ
Ý,ý
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Þ,þ
Ä,Æ,ä,æ
Ö,Ø,ö,ø
Å,å
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_latvian_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ
CH,Ch,cH,ch
Č,č
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ǧ,ǧ,Ǵ,ǵ
Ģ,ģ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
Y,y
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ǩ,ǩ
Ķ,ķ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ļ,ļ
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ņ,ņ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ř,ř
RR,Rr,rR,rr
Ŗ,ŗ
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,ſ
SS,Ss,sS,ss,ß
Š,š
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż
ƍ
Ž,ž
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_romanian_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Ã,Ä,Å,à,á,ã,ä,å,Ā,ā,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Ă,ă
Â,â
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Ï,ì,í,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
Î,î
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Š,š,ſ
SS,Ss,sS,ss,ß
Ş,ş
Ʃ
ƪ
T,t,Ť,ť
ƾ
Ţ,ţ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_slovenian_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ
CH,Ch,cH,ch
Č,č
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,ſ
SS,Ss,sS,ss,ß
Š,š
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż
ƍ
Ž,ž
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_polish_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Ą,ą
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ć,ć
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ě,ě
Ę,ę
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ń,ń
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ô,Õ,Ö,ò,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ó,ó
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ś,ś
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ž,ž
ƍ
Ź,ź
Ż,ż
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_estonian_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Å,à,á,â,ã,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz
Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,ò,ó,ô,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,ſ
SS,Ss,sS,ss,ß
Š,š
Z,z
Ž,ž
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,ù,ú,û,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
Õ,õ
Ä,ä
Ö,ö
Ü,ü
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Ź,ź,Ż,ż
ƍ
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_spanish_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ñ,ñ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_swedish_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,à,á,â,ã,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,ò,ó,ô,õ,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,ù,ú,û,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ü,Ý,ü,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Å,å
Ä,Æ,ä,æ
Ö,Ø,ö,ø
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_turkish_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ç,ç
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ğ,ğ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
I,ı
IJ,Ij
ƕ,Ƕ
Ħ,ħ
i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
iJ,ij,Ĳ,ĳ
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,ò,ó,ô,õ,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ö,ö
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Š,š,ſ
SS,Ss,sS,ss,ß
Ş,ş
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,ù,ú,û,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ü,ü
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_czech_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ
cH
Č,č
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
CH,Ch,ch
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ
RR,Rr,rR,rr
Ř,ř
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,ſ
SS,Ss,sS,ss,ß
Š,š
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż
ƍ
Ž,ž
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_danish_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,à,á,â,ã,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
aA
Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,ò,ó,ô,õ,Ō,ō,Ŏ,ŏ,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,ù,ú,û,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ü,Ý,ü,ý,ÿ,Ű,ű,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Ä,Æ,ä,æ
Ö,Ø,ö,ø,Ő,ő
AA,Aa,aa,Å,å
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_lithuanian_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,CH,Ch,c,ch,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ
cH
Č,č
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,Y,i,y,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,ſ
SS,Ss,sS,ss,ß
Š,š
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż
ƍ
Ž,ž
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_slovak_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Å,à,á,â,ã,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Ä,ä
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ
cH
Č,č
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
CH,Ch,ch
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Õ,Ö,ò,ó,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ô,ô
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,ſ
SS,Ss,sS,ss,ß
Š,š
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż
ƍ
Ž,ž
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_spanish2_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
cH
CH,Ch,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,i,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij,Ĳ,ĳ
ı
Ɨ
Ɩ
J,j,Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj,Ǉ,ǈ,ǉ
lL
LL,Ll,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj,Ǌ,ǋ,ǌ
Ñ,ñ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,RR,Rr,r,rr,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
rR
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
U,u,Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
V,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
select group_concat(c1 order by c1) from t1 group by c1 collate utf8_roman_ci;
group_concat(c1 order by c1)
÷
×
A,a,À,Á,Â,Ã,Ä,Å,à,á,â,ã,ä,å,Ā,ā,Ă,ă,Ą,ą,Ǎ,ǎ,Ǟ,ǟ,Ǡ,ǡ,Ǻ,ǻ
AA,Aa,aA,aa
Æ,æ,Ǣ,ǣ,Ǽ,ǽ
B,b
ƀ
Ɓ
Ƃ,ƃ
C,c,Ç,ç,Ć,ć,Ĉ,ĉ,Ċ,ċ,Č,č
CH,Ch,cH,ch
Ƈ,ƈ
D,d,Ď,ď
DZ,Dz,dZ,dz,Ǆ,ǅ,ǆ,Ǳ,ǲ,ǳ
Đ,đ
Ɖ
Ɗ
Ƌ,ƌ
Ð,ð
E,e,È,É,Ê,Ë,è,é,ê,ë,Ē,ē,Ĕ,ĕ,Ė,ė,Ę,ę,Ě,ě
Ǝ,ǝ
Ə
Ɛ
F,f
Ƒ,ƒ
G,g,Ĝ,ĝ,Ğ,ğ,Ġ,ġ,Ģ,ģ,Ǧ,ǧ,Ǵ,ǵ
Ǥ,ǥ
Ɠ
Ɣ
Ƣ,ƣ
H,h,Ĥ,ĥ
ƕ,Ƕ
Ħ,ħ
I,J,i,j,Ì,Í,Î,Ï,ì,í,î,ï,Ĩ,ĩ,Ī,ī,Ĭ,ĭ,Į,į,İ,Ǐ,ǐ
IJ,Ij,iJ,ij
Ĳ,ĳ
ı
Ɨ
Ɩ
Ĵ,ĵ,ǰ
K,k,Ķ,ķ,Ǩ,ǩ
Ƙ,ƙ
L,l,Ĺ,ĺ,Ļ,ļ,Ľ,ľ
Ŀ,ŀ
LJ,Lj,lJ,lj
Ǉ,ǈ,ǉ
LL,Ll,lL,ll
Ł,ł
ƚ
ƛ
M,m
N,n,Ñ,ñ,Ń,ń,Ņ,ņ,Ň,ň,Ǹ,ǹ
NJ,Nj,nJ,nj
Ǌ,ǋ,ǌ
Ɲ
ƞ
Ŋ,ŋ
O,o,Ò,Ó,Ô,Õ,Ö,ò,ó,ô,õ,ö,Ō,ō,Ŏ,ŏ,Ő,ő,Ơ,ơ,Ǒ,ǒ,Ǫ,ǫ,Ǭ,ǭ
OE,Oe,oE,oe,Œ,œ
Ø,ø,Ǿ,ǿ
Ɔ
Ɵ
P,p
Ƥ,ƥ
Q,q
ĸ
R,r,Ŕ,ŕ,Ŗ,ŗ,Ř,ř
RR,Rr,rR,rr
Ʀ
S,s,Ś,ś,Ŝ,ŝ,Ş,ş,Š,š,ſ
SS,Ss,sS,ss,ß
Ʃ
ƪ
T,t,Ţ,ţ,Ť,ť
ƾ
Ŧ,ŧ
ƫ
Ƭ,ƭ
Ʈ
Ù,Ú,Û,Ü,ù,ú,û,ü,Ũ,ũ,Ū,ū,Ŭ,ŭ,Ů,ů,Ű,ű,Ų,ų,Ư,ư,Ǔ,ǔ,Ǖ,ǖ,Ǘ,ǘ,Ǚ,ǚ,Ǜ,ǜ
Ɯ
Ʊ
U,V,u,v
Ʋ
W,w,Ŵ,ŵ
X,x
Y,y,Ý,ý,ÿ,Ŷ,ŷ,Ÿ
Ƴ,ƴ
Z,z,Ź,ź,Ż,ż,Ž,ž
ƍ
Ƶ,ƶ
Ʒ,Ǯ,ǯ
Ƹ,ƹ
ƺ
Þ,þ
ƿ,Ƿ
ƻ
Ƨ,ƨ
Ƽ,ƽ
Ƅ,ƅ
ŉ
ǀ
ǁ
ǂ
ǃ
drop table t1;
SET NAMES utf8;
CREATE TABLE t1 (c varchar(255) NOT NULL COLLATE utf8_general_ci, INDEX (c));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x039C03C903B403B11F770308 USING utf8));
SELECT * FROM t1 WHERE c LIKE CONVERT(_ucs2 0x039C0025 USING utf8)
COLLATE utf8_general_ci;
c
Μωδαί̈
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x039C03C903B4 USING utf8));
SELECT * FROM t1 WHERE c LIKE CONVERT(_ucs2 0x039C0025 USING utf8)
COLLATE utf8_general_ci ORDER BY c;
c
Μωδ
Μωδαί̈
DROP TABLE t1;
CREATE TABLE t1 (c varchar(255) NOT NULL COLLATE ucs2_unicode_ci, INDEX (c));
INSERT INTO t1 VALUES (_ucs2 0x039C03C903B403B11F770308);
SELECT * FROM t1 WHERE c LIKE _ucs2 0x039C0025 COLLATE ucs2_unicode_ci;
c
Μωδαί̈
INSERT INTO t1 VALUES (_ucs2 0x039C03C903B4);
SELECT * FROM t1 WHERE c LIKE _ucs2 0x039C0025
COLLATE ucs2_unicode_ci ORDER BY c;
c
Μωδ
Μωδαί̈
DROP TABLE t1;
CREATE TABLE t1 (c varchar(255) NOT NULL COLLATE utf8_unicode_ci, INDEX (c));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x039C03C903B403B11F770308 USING utf8));
SELECT * FROM t1 WHERE c LIKE CONVERT(_ucs2 0x039C0025 USING utf8) COLLATE utf8_unicode_ci;
c
Μωδαί̈
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x039C03C903B4 USING utf8));
SELECT * FROM t1 WHERE c LIKE CONVERT(_ucs2 0x039C0025 USING utf8)
COLLATE utf8_unicode_ci ORDER BY c;
c
Μωδ
Μωδαί̈
DROP TABLE t1;
CREATE TABLE t1 (
col1 CHAR(32) CHARACTER SET utf8 NOT NULL
);
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0041004100410627 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0041004100410628 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0041004100410647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0041004100410648 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0633064A0651062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062D06330646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A0642064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06320627062F0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062806310627064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064706450647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F062706460634062C0648064A06270646064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A90647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A06270631064A062E USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062706460642064406270628 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064A0631062706460650 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627062F064806270631062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280631062706480646200C06310627 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062E064806270646062F0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0648 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A062D062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A0623062B064A0631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06220646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0642063106270631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06AF06310641062A0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06270646062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0634062E0635064A0651062A064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0628062706310632 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06270633062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063906A90633 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06270648060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F0631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062D062F0648062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0628064A0633062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0648067E0646062C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06330627064406AF064A060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063306270644 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064606450627064A0646062F0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A06280631064A0632 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0645062C06440633 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280648062F060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0628064A0646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06350641062D0627062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064A0646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A9062A06270628 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x068606340645 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0645064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062E06480631062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0686064706310647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06420648064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06450635064506510645 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06310627 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0646063406270646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0645064A200C062F0647062F060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0647063106860646062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06390645064400BB USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A9064806340634 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0628 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064706500646064A064606AF USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627062D063306270646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064A062706310634062706370631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06450646062A06340631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0634062F0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F0633062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A064806270646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0647064506270646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064806510644 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A0634062E064A0635 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F0627062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280627 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A064106270648062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062D06270644062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A064106A906510631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063A064406280647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F06270631062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064A06A9064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06270632 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063106470628063106270646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064606470636062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064506340631064806370647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064A063106270646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0646064A0632 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064A06A9 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0645062D064206510642 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0637063106270632 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064106310647064606AF USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A0645062F06510646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064A063106270646064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280648062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A90627063106470627064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06270648 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0639063106350647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064506480631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0633064A06270633064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064A063106270646060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062D064806320647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063906440645 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F062706460634 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06450642062706440627062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F064A06AF0631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0648064A06980647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0646062706450647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064506480631062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0628062D062B USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0628063106310633064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064606480634062A0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06450646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A064606470627 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0622064606860647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F064806310647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064206270645062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x067E0631062F062706320645 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0698062706460648064A0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0648064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F06390648062A0650 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063306500631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F0646064A0633064F0646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063106270633 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0647064A0626062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0639064406480645200C063406310642064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280639062F0627064B USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0645062F063106330647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062206410631064A06420627064A064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F06270646063406AF06270647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06440646062F0646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x067E064A06480633062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0647064606AF06270645064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x067E0633 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0622063A06270632 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062C064606AF USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062C064706270646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F064806510645 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063406470631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A9064506280631064A062C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06450646062A06420644 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A90631062F0646062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06470645 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06310641062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06220646062C0627 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064506270646062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A0627 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062706A9062A06280631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064606380631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F06480644062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F06480628062706310647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064606330628062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0645063306270639062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0634062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06480632064A0631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0645062E062A06270631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06330641064A0631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627064606AF0644064A0633 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A0642064A200C06320627062F0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280627063206AF0634062A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0647064506330631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06220644064506270646064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06270634 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06220645062F0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A906270631064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x067E0631062F0627062E062A0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063906440645064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x0627062F0628064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062D062F0651 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064606280648062F060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06480644064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x063906480636060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06340627064A062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064506470645 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062A0631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06220646060C USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06470645063306310634 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A90627064606480646 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062E0627064606480627062F06AF064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06AF06310645064A USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280648062C0648062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062206480631062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F0648 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06A90627064506440627064B USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x064A06A9062F064A06AF0631 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06AF USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x062F064406280633062A0647 USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06280648062F0646062F USING utf8));
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x06450647064506270646 USING utf8));
SELECT HEX(CONVERT(col1 USING ucs2)) FROM t1 ORDER BY col1 COLLATE utf8_persian_ci, col1 COLLATE utf8_bin;
HEX(CONVERT(col1 USING ucs2))
0041004100410627
0041004100410628
0041004100410648
0041004100410647
0622063A06270632
062206410631064A06420627064A064A
06220644064506270646064A
06220645062F0647
06220646
06220646060C
06220646062C0627
0622064606860647
062206480631062F
0627062D063306270646
0627062F0628064A
0627062F064806270631062F
06270632
06270633062A
06270634
0627064206270645062A
062706A9062A06280631
0627064506480631
06270646062F
062706460642064406270628
0627064606AF0644064A0633
06270648
06270648060C
0627064806510644
0627064A
0627064A063106270646
0627064A063106270646060C
0627064A0631062706460650
0627064A063106270646064A
0627064A0646
0628
06280627
0628062706310632
06280627063206AF0634062A
0628062D062B
06280631062706480646200C06310627
062806310627064A
0628063106310633064A
06280639062F0627064B
06280648062C0648062F
06280648062F
06280648062F060C
06280648062F0646062F
06280647
0628064A0633062A
0628064A0646
067E0631062F0627062E062A0647
067E0631062F062706320645
067E0633
067E064A06480633062A
062A0627
062A06270631064A062E
062A0623062B064A0631
062A06280631064A0632
062A062D062A
062A0631
062A0634062E064A0635
062A064106270648062A
062A064106A906510631
062A0642064A
062A0642064A200C06320627062F0647
062A0645062F06510646
062A064606470627
062A064806270646
062C064606AF
062C064706270646
068606340645
0686064706310647
062D06270644062A
062D062F0651
062D062F0648062F
062D06330646
062D064806320647
062E0627064606480627062F06AF064A
062E064806270646062F0647
062E06480631062F
062F0627062F
062F06270631062F
062F062706460634
062F062706460634062C0648064A06270646064A
062F06270646063406AF06270647
062F0631
062F0633062A
062F06390648062A0650
062F064406280633062A0647
062F0646064A0633064F0646
062F0648
062F06480628062706310647
062F064806310647
062F06480644062A
062F064806510645
062F064A06AF0631
06310627
063106270633
06310641062A
063106470628063106270646
06320627062F0647
0698062706460648064A0647
063306500631
063306270644
06330627064406AF064A060C
06330641064A0631
0633064A06270633064A
0633064A0651062F
06340627064A062F
0634062E0635064A0651062A064A
0634062F
0634062F0647
063406470631
06350641062D0627062A
0637063106270632
0639063106350647
063906A90633
063906440645
063906440645064A
0639064406480645200C063406310642064A
06390645064400BB
063906480636060C
063A064406280647
064106310647064606AF
0642063106270631
06420648064A
06A90627063106470627064A
06A906270631064A
06A90627064506440627064B
06A90627064606480646
06A9062A06270628
06A90631062F0646062F
06A9064506280631064A062C
06A9064806340634
06A90647
06AF
06AF06310641062A0647
06AF06310645064A
06440646062F0646
064506270646062F
0645062C06440633
0645062D064206510642
0645062E062A06270631
0645062F063106330647
0645063306270639062F
064506340631064806370647
06450635064506510645
06450642062706440627062A
06450646
06450646062A06340631
06450646062A06420644
064506480631062F
064506470645
06450647064506270646
0645064A
0645064A200C062F0647062F060C
0646062706450647
064606280648062F060C
064606330628062A
0646063406270646
064606380631
064606450627064A0646062F0647
064606480634062A0647
064606470636062A
0646064A0632
0648
0648067E0646062C
06480632064A0631
06480644064A
0648064A
0648064A06980647
064706500646064A064606AF
0647063106860646062F
06470645
0647064506270646
0647064506330631
06470645063306310634
064706450647
0647064606AF06270645064A
0647064A0626062A
064A062706310634062706370631
064A06A9
064A06A9062F064A06AF0631
064A06A9064A
DROP TABLE t1;
SET @test_character_set= 'utf8';
SET @test_collation= 'utf8_swedish_ci';
SET @safe_character_set_server= @@character_set_server;
SET @safe_collation_server= @@collation_server;
SET character_set_server= @test_character_set;
SET collation_server= @test_collation;
CREATE DATABASE d1;
USE d1;
CREATE TABLE t1 (c CHAR(10), KEY(c));
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
c	char(10)	utf8_swedish_ci	YES	MUL	NULL			
INSERT INTO t1 VALUES ('aaa'),('aaaa'),('aaaaa');
SELECT c as want3results FROM t1 WHERE c LIKE 'aaa%';
want3results
aaa
aaaa
aaaaa
DROP TABLE t1;
CREATE TABLE t1 (c1 varchar(15), KEY c1 (c1(2)));
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
c1	varchar(15)	utf8_swedish_ci	YES	MUL	NULL			
INSERT INTO t1 VALUES ('location'),('loberge'),('lotre'),('boabab');
SELECT c1 as want3results from t1 where c1 like 'l%';
want3results
location
loberge
lotre
SELECT c1 as want3results from t1 where c1 like 'lo%';
want3results
location
loberge
lotre
SELECT c1 as want1result  from t1 where c1 like 'loc%';
want1result
location
SELECT c1 as want1result  from t1 where c1 like 'loca%';
want1result
location
SELECT c1 as want1result  from t1 where c1 like 'locat%';
want1result
location
SELECT c1 as want1result  from t1 where c1 like 'locati%';
want1result
location
SELECT c1 as want1result  from t1 where c1 like 'locatio%';
want1result
location
SELECT c1 as want1result  from t1 where c1 like 'location%';
want1result
location
DROP TABLE t1;
DROP DATABASE d1;
USE test;
SET character_set_server= @safe_character_set_server;
SET collation_server= @safe_collation_server;
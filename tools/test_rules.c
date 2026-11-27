/* The substitution rules of the text compare (textlib.asm rules_apply): ordinary rules old=new with their
   small patterns, and regular expression rules /pattern/=new (regex.inc). Linked to the production object;
   rules_apply is made global for this test with objcopy. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
extern uint64_t rules_apply(const wchar_t*src,wchar_t*dst,uint64_t cap);
extern const wchar_t*g_tc_rules;
int tl_convert(const wchar_t*p,wchar_t*o){(void)p;(void)o;return 0;}
int tl_isconv(const wchar_t*p){(void)p;return 0;}
static int checks,failed;
static void rule(const wchar_t*rules,const wchar_t*in,const wchar_t*want,const char*name){
 wchar_t out[1200];for(int i=0;i<1200;i++)out[i]=L'#';
 g_tc_rules=rules;uint64_t n=rules_apply(in,out,1000);
 int ok=n==wcslen(want)&&!out[n]&&!wcscmp(out,want);
 checks++;printf("%s %s\n",ok?"PASS":"FAIL",name);
 if(!ok){failed++;char a[600],b[600];WideCharToMultiByte(CP_UTF8,0,out,-1,a,600,NULL,NULL);WideCharToMultiByte(CP_UTF8,0,want,-1,b,600,NULL,NULL);printf("  got \"%s\" (%llu), expected \"%s\"\n",a,(unsigned long long)n,b);}
 fflush(stdout);
}
int main(void){
 SetConsoleOutputCP(CP_UTF8);
 /* ---- ordinary rules stay as they were */
 rule(L"count=total",L"int count = 10",L"int total = 10","ordinary rule: text for text");
 rule(L"// \\w+=// sum",L"x // total",L"x // sum","ordinary rule that starts with slashes is not a regular expression");
 rule(L"a+b=X",L"aaab ab b",L"X X b","ordinary rule: + behind a character");
 rule(L"*;=V",L"v1.2; w",L"V w","ordinary rule: a * with nothing before it is any run");
 rule(L"v*;=V",L"v1.2; vv; w",L"v1.2V V w","ordinary rule: * behind a character is none or more of it");
 rule(L"/a/b=c",L"x/a/b",L"xc","a slash rule without = behind the closing slash is an ordinary rule");
 rule(L"foo=bar;x=y",L"foo x",L"bar y","several ordinary rules");
 /* ---- regular expressions */
 rule(L"/\\d+ms/=",L"took 123ms ok",L"took  ok","digits, empty replacement: the match is ignored");
 rule(L"/\\d{4}-\\d{2}-\\d{2}/=DATE",L"at 2026-11-27 and 26-1-1",L"at DATE and 26-1-1","exact counts");
 rule(L"/[0-9a-f]{8}/=HASH",L"id deadbeef x",L"id HASH x","class with ranges and a count");
 rule(L"/[^ ]+@[^ ]+/=MAIL",L"to a.b@c.d now",L"to MAIL now","negated class");
 rule(L"/^#.*$/=",L"# comment",L"","whole line: start and end anchors");
 rule(L"/^#.*$/=",L"x # comment",L"x # comment","the start anchor does not match inside the line");
 rule(L"/colou?r/=C",L"color colour colouur",L"C C colouur","optional character");
 rule(L"/a.c/=X",L"abc a-c ac",L"X X ac","dot is exactly one character");
 rule(L"/\\s+$/=",L"x  y   ",L"x  y","end anchor: trailing blanks only");
 rule(L"/x=\\d;/=Y",L"x=1; x=2;",L"Y Y","= and ; inside the pattern");
 rule(L"/\\/\\/.*/=",L"a // b",L"a ","escaped slashes");
 rule(L"/\\D+/=N",L"ab12cd",L"N12N","not a digit");
 rule(L"/\\S+/=W",L"ab  cd",L"W  W","not a blank");
 rule(L"/\\W+/=_",L"a, b;c",L"a_b_c","not a word character");
 rule(L"/\\w+\\(\\)/=F",L"call foo() bar",L"call F bar","escaped brackets");
 rule(L"/a{2,3}/=X",L"a aa aaa aaaa",L"a X X Xa","count range");
 rule(L"/a{2,}/=X",L"a aaaa",L"a X","open count range");
 rule(L"/a{0,1}b/=X",L"ab b aab",L"X X aX","count range from none");
 rule(L"/[a-c-]+/=X",L"a-b d",L"X d","a - before the closing bracket is itself");
 rule(L"/[\\d.]+/=N",L"v1.2.3 x",L"vN x","class with an escape and a dot");
 rule(L"/[\\]x]+/=B",L"a]x]b",L"aBb","escaped closing bracket in a class");
 rule(L"/a.*c/=X",L"abcbc d",L"X d","greedy");
 rule(L"/a.*c d/=X",L"abcbc d",L"X","greedy gives back what the rest needs");
 rule(L"/\\d+\\d\\d/=N",L"12345 12",L"N 12","backtracking over a + ");
 rule(L"/x*/=Y",L"bxxb",L"bYb","an empty match changes nothing and ends");
 rule(L"/x*/=Y",L"bb",L"bb","only empty matches: the line stays");
 rule(L"/^/=Y",L"ab",L"ab","an anchor alone matches nothing");
 rule(L"/\\t+/= ",L"a\t\tb",L"a b","tab");
 rule(L"/[가-힣]+/=K",L"값 테스트 x",L"K K x","range outside ASCII");
 rule(L"/\\w+/=W",L"값_1 x",L"W W","word characters include letters outside ASCII");
 rule(L"/\\d+/=N;foo=bar",L"foo 12",L"bar N","a regular expression and an ordinary rule together");
 rule(L"foo=bar;/\\d+/=N",L"foo 12",L"bar N","in the other order");
 rule(L"/a;b/=X;/c/=Y",L"a;b c",L"X Y","a ; in a pattern does not end the rule");
 rule(L"/\\d+/=<$&>",L"a1",L"a<$&>",  "the replacement is plain text");
 /* ---- patterns that are not well formed never fail the compare */
 rule(L"/a[b/=X",L"a[b a",L"X a","an unclosed bracket is a literal one");
 rule(L"/a{x}/=Z",L"a{x} a",L"Z a","a brace that is not a count is a literal one");
 rule(L"/a{2/=Z",L"a{2 aa",L"Z aa","an unclosed count is literal");
 rule(L"/ab\\/=X",L"ab\\ x",L"ab\\ x","a pattern that ends in a backslash before the slash is no rule at all");
 rule(L"/+a/=X",L"+a a",L"X a","a quantifier with nothing before it is itself");
 rule(L"/abc",L"/abc",L"/abc","an unfinished rule does nothing");
 /* ---- the output never exceeds the room it was given */
 {wchar_t in[40],out[64];for(int i=0;i<30;i++)in[i]=L'a';in[30]=0;for(int i=0;i<64;i++)out[i]=L'#';
  g_tc_rules=L"/a/=xxxxxxxx";uint64_t n=rules_apply(in,out,40);
  int ok=n<=40&&!out[n]&&out[42]==L'#';checks++;printf("%s %s\n",ok?"PASS":"FAIL","a replacement that would overflow the buffer is not made");if(!ok)failed++;}
 printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}

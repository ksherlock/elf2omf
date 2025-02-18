

void puts(const char *cp) {

  __asm(
	" pei dp: _Dp+2\n"
	" pei dp: _Dp+0\n"
	" ldx ##0x200c\n"
	" jsl 0xe10000\n"
	: /* output */
	: //[cp]"Kdp32" (cp) /* input */
	: "c", "x", "y" /* clobbered */);


	__asm(
	"; pea #0x0d\n"
	" pha\n"
	" ldx ##0x180c\n"
	" jsl 0xe10000\n"
	: /* output */
	: "Kc"('\r')/* input */
	: "c", "x", "y" /* clobbered */);
}


int main(int argc, char **argv) {
  puts("hello, world!");
  return 0;
}

static int foo(int i) {
	return(i+1);
}

int main(void) {
	volatile int i;
	volatile int t = 0;

	for(i = 0; i < 100000000; i++)
	    t += foo(i);

}

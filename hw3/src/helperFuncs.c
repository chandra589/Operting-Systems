#include "helperFuncs.h"

int FibNum(int n)
{
	if (n == 1) return 1;
	else if (n == 2) return 2;
	else
	{
		int a = 1, b = 2;
		int c = 0;
		while (n > 2)
		{
			c = a + b;
			a = b;
			b = c;
			n--;
		}
		return c;
	}
	return 0;
}
#include "helper_Funcs.h"
#include "debug.h"


/* compares two strings.
return 0 if strings are not same
return 1 if strings are same */

int stringcompare(char* c1, char* c2)
{
	int cnt = 0;
	int flag = 1;
	while (*(c1+cnt) != '\0' && *(c2+cnt) != '\0')
	{
		if (*(c1+cnt) != *(c2+cnt))
		{
			flag = 0;
			break;
		}
		cnt++;
	}

	if (flag == 0) return flag;
	if (*(c1+cnt) == '\0' && *(c2+cnt) == '\0')
	{
		//debug("Strings are same\n");
	    return 1;
	}
	return 0;
}

int stringcompareforbytes(char* c)
{
	int cnt = 0;
	while (*(c+cnt) != '\0')
		cnt++;

	//all characters aparts from units should be 0
	for (int i = 0; i < cnt - 1; i++)
	{
		if (*(c + i) != '0')
			return 0;
	}

	if (*(c + cnt - 1) == '1') return 1;
	else if (*(c + cnt - 1) == '2') return 2;
	else
		return 0;

	return 0;
}



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

int FibLessthanGivenNumber(char* c, int* counter, int n)
{
	if (n == 1)
	{
		if ((*counter)+1 > 8)
		{
			//debug("n = 1, put-char, before counter value: %d", *counter);
			//debug("put-char value: %hhx\n", *c);
			putchar(*c);
			*c = 0;
			*counter = 1;
			*c |= 1 << (8 - *counter);
		}
		else
		{
			//debug("n = 1, no put-char, before counter value: %d", *counter);
			(*counter) = (*counter) + 1;
			*c |= 1 << (8 - *counter);
		}
		return 1;
	}
	else if (n == 2)
	{
		if ((*counter)+2 > 8)
		{
			//debug("n = 2, put-char, before counter value: %d", *counter);
			//debug("putchar value: %hhx\n", *c);
			putchar(*c);
			*c = 0;
			*counter = *counter + 2 - 8;
			*c |= 1 << (8 - *counter);
		}
		else
		{
			//debug("n = 2, no put-char, before counter value: %d", *counter);
			(*counter) = (*counter) + 2;
			*c |= 1 << (8 - *counter);
		}
		return 2;
	}
	else
	{
		int i = 2;
		while (n > 2)
		{
			if ((FibNum(i) <= n) && (FibNum(i+1) > n))
			{
				int prevFibindex = FibLessthanGivenNumber(c, counter, n - FibNum(i));
				if (i > 2)
				{
					if ((*counter)+ (i - prevFibindex) > 8)
					{
						//debug("i & prev-FibIndex, put-char, before counter value: %d %d %d", i, prevFibindex, *counter);
						//debug("putchar value: %hhx\n", *c);
						putchar(*c);
						*c = 0;
						*counter = *counter + (i - prevFibindex) - 8;

						if (*counter > 8)
						{
							while (*counter > 8)
							{
								//debug("putchar value: %hhx\n", *c);
								putchar(*c);
								*counter = *counter - 8;
							}

							*c |= 1 << (8 - *counter);
						}
						else
						{
							*c |= 1 << (8 - *counter);
						}
					}
					else
					{
						//debug("i & prev-FibIndex, no put-char, before counter value: %d %d %d", i, prevFibindex, *counter);
						(*counter) = (*counter) + i - prevFibindex;
						*c |= 1 << (8 - *counter);
					}
				}
				return i;
			}
			i++;
		}

	}
	return 0;
}

int IsPowerofTwo(int num, int *depth, int *Enc_oddvalue)
{
	if (num == 1)
	{
		if ((*Enc_oddvalue) == 0)
			return 1;
		else
			return 0;
	}
	int ret = 0;
	int rem = num % 2;
	if (rem == 1)
	{
		*Enc_oddvalue = 1;
		(*depth)++;
		ret = IsPowerofTwo((num-1)/2, depth, Enc_oddvalue);
	}
	else
	{
		(*depth)++;
		ret = IsPowerofTwo(num/2, depth, Enc_oddvalue);
	}
	return ret;
}

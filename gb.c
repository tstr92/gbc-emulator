int mystrcpy(char *dst, char*src)
{
    int i = 0;
    while (src[i])
    {
        dst[i] = src[i];
        i++;
    }
    return i;
}

int strputint(int num, char* dst)
{
    int mult = 10;
    int i = 0;

    while (0 != (num/mult))
    {
        mult *= 10;
    }
    
    while (1 < mult)
    {
        mult /= 10;
        dst[i++] = "0123456789"[(num/mult) % 10];
    }

    return i;
}

void putc(unsigned char c)
{
    *((unsigned char*)0xE000) = c;
}

void puts(unsigned char *s)
{
    while (*s)
    {
        putc(*s);
        s++;
    }
}

unsigned char *FizzBuzz(int val)
{
    static unsigned char ret[20];
    int i = 0;

    if (0 != val)
    {
        if (0 == (val % 3))
        {
            i += mystrcpy(&ret[i], "Fizz");
        }

        if (0 == (val % 5))
        {
            i += mystrcpy(&ret[i], "Buzz");
        }
    }

    if (0 == i)
    {
        i += strputint(val, &ret[i]);
    }

    ret[i++] = '\n';
    ret[i++] = '\0';

    return ret;
}

void main(void)
{
    for(int i = 0; i < 100; i++)
    {
        char *data = FizzBuzz(i);
        puts(data);
    }
    __asm__("stop");
    return;
}
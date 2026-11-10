// EXPECTED : 51

int test(int x, int y)
{
    int d = x - y;
    int a = 0;

    if (d < 0)
    {
        a = a + 1;
    }

    if (d < 0)
    {
        a = a + 2;
    }

    if (d == 0)
    {
        a = a + 4;
    }

    if (d == 0)
    {
        a = a + 8;
    }

    int p = d * 2;

    if (p < 0)
    {
        a = a + 16;
    }

    if (p < 0)
    {
        a = a + 32;
    }

    return a;
}

int main()
{
    int r = test(10, 20);

    return r;
}

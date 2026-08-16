int add(int a, int b)
{
    return a + b;
}

int test()
{
    int x = 10;
    int y = 5;
    int z;
    int i;
    int w;
    int c;

    while (i < 100)
    {
        z = x + y;

        w = -z + i;

        c = add(z, w);

        i = i + 1;
    }

    return z + w;
}

int main()
{
    return test();
}

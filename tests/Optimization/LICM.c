int test()
{
    int x;
    int y;
    int z;
    int i;

    x = 10;
    y = 5;
    z = 0;
    i = 0;

    while (i < 100)
    {
        z = x + y;

        i = i + 1;
    }

    return z;
}

int main()
{
    return test();
}

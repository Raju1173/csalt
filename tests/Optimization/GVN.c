int add(int a, int b)
{
    return a + b;
}

int testAcyclicPhi(int cond)
{
    int x;

    if (cond == 1)
    {
        x = 100;
    }

    if (cond == 0)
    {
        x = 100;
    }

    return x + 5;
}

int testCyclicPhi()
{
    int val = 99;
    int i = 0;

    while (i < 10)
    {
        int temp = val;
        val = temp;

        i = i + 1;
    }

    return val;
}

int test()
{
    int t1 = add(3, 4);

    int t2 = t1;
    int t3 = t2;
    int t4 = t3;

    return 0 + t4;
}

int main()
{
    int x = 1 + 2 + add(3, 4);

    int y;

    y = 1 - 2;

    if (x == 1)
    {
        int a = 2 + 1;

        a = 2 - 1;
    }

    add(3, 4);

    if (x == 0)
    {
        int b = 1 + 2;

        b = 1 - 2;
    }

    y = add(2 + 1, 1 - 2);

    y = add(2 + 1, 1 - 2);

    int r1 = testAcyclicPhi(x);
    int r2 = testCyclicPhi();

    return r1 + r2 + add(3, 4);
}

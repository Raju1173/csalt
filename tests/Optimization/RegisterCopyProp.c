// EXPECTED : 36

int compute(int p1, int p2)
{
    int a = p1;
    int b = a;
    int c = b;
    int res = c + p2;

    int x = p1;
    int y = x;
    x = p2 + 10;
    res = res + y;

    int cond = p1 - p2;
    int z = p2;

    if (cond)
    {
        z = p1;
        res = res + z;
    }

    res = res + z;

    int i = 0;
    int k = p1;
    int acc = 0;

    while (i < 5)
    {
        acc = acc + k;
        k = p2;
        i = i + 1;
    }

    return res + acc;
}

int main()
{
    int first = 4;
    int second = first;

    int third = compute(first, second);

    int out = third;

    return out;
}

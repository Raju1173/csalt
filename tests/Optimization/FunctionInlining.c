// EXPECTED : 80

int add(int a, int b)
{
    return a + b;
}

int square(int x)
{
    return x * x;
}

int sumOfSquares(int x, int y)
{
    return add(square(x), square(y));
}

int poly(int x)
{
    int t1 = square(x);
    int t2 = square(x + 1);

    return add(t1, t2);
}

int absolute(int val)
{
    if (val < 0)
    {
        return -val;
    }

    return val;
}

int main()
{
    int a = 3;
    int b = 4;

    return sumOfSquares(a, b) + poly(2) + absolute(-42);
}

int main()
{
    int x = a + b;

    if(x > c + d)
    {
        hello(x, d);

    }

    while(a < b)
    {
        a = a + 1;

        if(a > c)
        {
            b = b - 1;

            hello(a, b);
        }

        b = b + 1;
    }

    return hello(x, b);
}

int hello(int a, int b)
{
    return 0;
}

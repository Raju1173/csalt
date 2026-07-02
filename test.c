int hello(int x, int y, int z)
{
    return x + y * z;
}

int main()
{
    int x = 0;
    int y = 0;

    while (a > 1 + 2 * hello(1, 2, hello(1, 2, 3)))
    {
        if (b == 4)
        {
            x = x + 1;
        }

        if (c != 3)
        {
            y = y + 1;
        }

        if (d >= 2)
        {
            x = y;
        }

        hello(x, y, x + y * x);
    }

    return x + y;
}

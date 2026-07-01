int main()
{
    int x = 0;
    int y = 0;

    while (a)
    {
        if (b)
        {
            x = x + 1;
        }

        if(c)
        {
            y = y + 1;
        }

        if (d)
        {
            x = y;
        }

        hello(x, y, x + y * x);
    }

    return x + y;
}

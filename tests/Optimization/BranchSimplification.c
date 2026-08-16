int main()
{
    int x = 0;

    if (1 > 2)
    {
        x = 1;
    }

    if (x > x)
    {
        x = 2;
    }

    if (x == x)
    {
        x = 3;
    }

    if (-x > -x)
    {
        x = 4;
    }

    return x;
}

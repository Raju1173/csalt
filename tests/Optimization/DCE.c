int add(int a, int b)
{
    return a + b;
}

int main()
{
    add(1, 2);

    int a = 2 + 3 + add(4, 5) * (0 - 1);

    if (4 > 6)
    {
        return 1;
    }

    return 0;
}

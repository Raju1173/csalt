int fact(int n, int o, int p, int q, int r, int s, int t, int u)
{
    if (n <= 1)
    {
        return 1;
    }

    return n * fact(n - 1, 1, 2, 3, 4, 5, 6, 7);
}

int main()
{
    return fact(5, 2, 3, 4, 4, 5, 6, 7);
}

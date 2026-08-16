int fib(int n)
{
    if (n <= 1)
    {
        return n;
    }

    return fib(n - 1) + fib(n - 2);
}

int main()
{
    int a = -2;

    if (-a - 1 + -2 + -(3 + 4) == -8)
    {
        return -fib(20); // Expected : -6765
    }
}

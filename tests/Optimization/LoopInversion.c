// EXPECTED : 55

int test(int n)
{
    int total = 0;
    int i = 1;

    while (i <= n)
    {
        total = total + i;
        i = i + 1;
    }

    return total;
}

int main()
{
    return test(10);
}

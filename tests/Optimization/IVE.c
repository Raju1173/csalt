// EXPECTED : 1337

int f(int n)
{
    int i = 0;
    int s = 0;

    while (i != n)
    {
        n = n - 1;
        s = s + 7;
        i = i + 1;
    }

    return s;
}

int main()
{
    int i = 0;
    int s = 0;
    int sum = 0;

    while (i != 20)
    {
        sum = sum + s;
        i = i + 1;
        s = s + 7;
    }

    return sum + f(2);
}

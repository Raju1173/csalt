// EXPECTED : 0

int main()
{
    int a = 0;

    if (-a)
    {
        a = 3;
    }

    int b;

    if (a == 3)
    {
    }

    int c = 0;

    return c;
}

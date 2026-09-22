// EXPECTED : 225

int main()
{
    int sum = 0;

    int i = 0;

    int factor = 5;

    while (i < 10)
    {
        sum = sum + i * factor;

        i = i + 1;
    }

    return sum;
}

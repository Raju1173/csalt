// EXPECTED : 105

int test(int start, int step, int N)
{
    int i = 0;
    int j = 0;
    int k = 0;

    int p = start;
    int q = start;

    int diffInit = 1;
    int diffStep = 0;

    int div1 = 0;
    int div2 = 0;

    while (i < N)
    {
        div1 = i * 4 + 16;
        div2 = j * 4 + 16;

        i = i + 1;
        j = j + 1;
        k = k + 1;

        p = p + step;
        q = q + step;

        diffInit = diffInit + 1;
        diffStep = diffStep + 2;
    }

    return i + j + k + p + q + div1 + div2 + diffInit + diffStep;
}

int main()
{
    return test(10, -1, 5);
}

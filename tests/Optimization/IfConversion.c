// EXPECTED : 4

int max(int a, int b)
{
    int res;

    if (a > b)
    {
        res = a;
    }

    if (a <= b)
    {
        res = b;
    }

    return res;
}

int absDiff(int a, int b)
{
    int diff = a - b;

    if (diff < 0)
    {
        diff = -diff;
    }

    return diff;
}

int clamp(int val, int low, int high)
{
    int res = val;

    if (res < low)
    {
        res = low;
    }

    if (res > high)
    {
        res = high;
    }

    return res;
}

int conditionalScale(int x, int cond)
{
    int factor = 1;

    if (cond != 0)
    {
        factor = x * 2 + 1;
    }

    if (cond == 0)
    {
        factor = x + 4;
    }

    return factor;
}

int main()
{
    int score = 0;

    if (max(10, 20) == 20)
    {
        score = score + 1;
    }

    if (absDiff(5, 12) == 7)
    {
        score = score + 1;
    }

    if (clamp(15, 0, 10) == 10)
    {
        score = score + 1;
    }

    if (conditionalScale(3, 1) == 7)
    {
        score = score + 1;
    }

    return score;
}

// EXPECTED : 5

int heuristicTest(int cond)
{
    int v1 = cond + 1;
    int v2 = cond + 2;
    int v3 = cond + 3;
    int v4 = cond + 4;
    int v5 = cond + 5;
    int v6 = cond + 6;
    int v7 = cond + 7;
    int v8 = cond + 8;
    int v9 = cond + 9;
    int v10 = cond + 10;
    int v11 = cond + 11;
    int v12 = cond + 12;
    int v13 = cond + 13;
    int v14 = cond + 14;
    int v15 = cond + 15;
    int v16 = cond + 16;

    int res;

    if (cond > 0)
    {
        res = 10;
    }

    if (cond <= 0)
    {
        res = 20;
    }

    int sum1 = res + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16;

    int sum2 = 0;

    int i = 0;

    while (i < 10)
    {
        if (cond > 0)
        {
            sum2 = sum2 + 1;
        }

        if (cond <= 0)
        {
            sum2 = sum2 + 2;
        }

        i = i + 1;
    }

    return sum1 + sum2;
}

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

    if (heuristicTest(1) == 172)
    {
        score = score + 1;
    }

    return score;
}

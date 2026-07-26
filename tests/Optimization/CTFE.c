int absoluteValue(int val)
{
    if (val < 0)
    {
        return -val;
    }

    return val;
}

int computeGCD(int a, int b)
{
    int tempA = absoluteValue(a);

    int tempB = absoluteValue(b);

    while (tempB != 0)
    {
        int remainder = tempA;

        while (remainder >= tempB)
        {
            remainder = remainder - tempB;
        }

        tempA = tempB;
        tempB = remainder;
    }

    return tempA;
}

int test(int limit, int targetGCD)
{
    int totalMatches = 0;

    int x = 1;

    while (x <= limit)
    {
        int y = 1;

        while (y <= limit)
        {
            if (computeGCD(x, y) == targetGCD)
            {
                totalMatches = totalMatches + 1;
            }

            y = y + 1;
        }

        x = x + 1;
    }

    return totalMatches;
}

int main()
{
    return test(12, 3);
}

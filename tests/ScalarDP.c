int countpaths(int targetsteps)
{
    int dp0;
    int dp1;
    int dp2;
    int current;
    int step;

    dp0 = 1;
    dp1 = 1;
    dp2 = 2;

    step = 3;

    if (targetsteps == 0)
    {
        return dp0;
    }

    if (targetsteps == 1)
    {
        return dp1;
    }

    if (targetsteps == 2)
    {
        return dp2;
    }

    // BUG (fixed) : assembly generator emmitted "cmp StackValue, StackValue"
    while (step < targetsteps + 1)
    {
        current = dp0 + dp1 + dp2;

        dp0 = dp1;
        dp1 = dp2;
        dp2 = current;

        step = step + 1;
    }

    return current;
}

int main()
{
    return countpaths(15); //Expected : 5768
}

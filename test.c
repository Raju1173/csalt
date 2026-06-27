int StraightLine()
{
    x = 0;
    x = 1;
    return x;
}

int OneSidedDefinition()
{
    x = 0;

    if (cond == true)
    {
        x = 1;
    }

    return x;
}

int TwoVariables()
{
    x = 0;
    y = 0;

    if (cond == false)
    {
        x = 1;
        y = 2;
    }

    return x + y;
}

int LocalVariable()
{
    x = 0;

    if (cond)
    {
        a = 1;
        x = 2;
    }

    return x;
}

int NestedIf()
{
    x = 0;

    if (a)
    {
        x = 1;

        if (b)
        {
            x = 2;
        }
    }

    return x;
}

int IndependentVariables()
{
    x = 0;
    y = 0;

    if (a)
    {
        x = 1;
    }

    if (b)
    {
        y = 2;
    }

    return x + y;
}

int Loop()
{
    x = 0;

    while (cond)
    {
        x = x + 1;
    }

    return x;
}

int LoopWithNestedIf()
{
    x = 0;

    while (cond)
    {
        if (a)
        {
            x = x + 1;
        }
    }

    return x;
}

int SequentialLoops()
{
    x = 0;

    while (a)
    {
        x = 1;
    }

    while (b)
    {
        x = 2;
    }

    return x;
}

int FinalTest()
{
    x = 0;

    if (a)
    {
        x = 1;
    }

    while (b)
    {
        if (c)
        {
            x = x + 1;
        }
    }

    return x;
}

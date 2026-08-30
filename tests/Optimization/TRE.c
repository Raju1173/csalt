// EXPECTED : 126

int findVal(int x, int y)
{
    if (x > 100)
    {
        return findVal(x / 2, y + 1);
    }

    if (y > 100)
    {
        return findVal(x + 1, y / 2);
    }

    return x + y;
}

int main()
{
    return findVal(200, 300);
}

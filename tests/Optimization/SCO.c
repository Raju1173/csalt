// EXPECTED : 21

int addScaled(int x, int y)
{
    return x + (y * 2);
}

int processData(int a, int b)
{
    int temp1 = a + 5;
    int temp2 = b * 3;

    return addScaled(temp1, temp2);
}

int main()
{
    return processData(4, 2);
}

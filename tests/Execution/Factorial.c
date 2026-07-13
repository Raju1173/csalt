// BUG (fixed) : parameters were not being initialized explicitly in any of the IRs, so phi insertion got messed up if the only definition coming of a variable from the entry block was the parameter, which the phi insertion algo couldnt see...
int factorial(int n)
{
    int res;
    int counter;
    int temp;

    res = 1;

    while (n > 1)
    {
        counter = n;
        temp = 0;

        while (counter > 0)
        {
            temp = temp + res;
            counter = counter - 1;
        }

        res = temp;
        n = n - 1;
    }

    return res;
}

int main()
{
    return factorial(12); // max number whose factorial fits in 32 bits...
}

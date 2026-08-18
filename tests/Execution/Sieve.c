// EXPECTED : 37

int getLastPrime(int maxLimit)
{
    int currentNum;
    int divisor;
    int isPrime;
    int foundPrime;

    currentNum = maxLimit;
    foundPrime = 0;

    while (currentNum > 1)
    {
        if (foundPrime == 0)
        {
            isPrime = 1;
            divisor = 2;

            while (divisor * divisor * 1 <= currentNum)
            {
                if (isPrime == 1)
                {
                    // BUG (fixed) : parser didnt handle EXPR node encountering a left paranthesis
                    if ((currentNum / divisor) * divisor == currentNum)
                    {
                        isPrime = 0;
                    }
                }

                divisor = divisor + 1;
            }

            if (isPrime == 1)
            {
                foundPrime = currentNum;
            }
        }

        currentNum = currentNum - 1;
    }

    return foundPrime;
}

int main()
{
    return getLastPrime(40);
}

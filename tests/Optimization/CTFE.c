int absolute_value(int val)
{
    if (val < 0)
    {
        return 0 - val;
    }
    return val;
}

int compute_gcd(int a, int b)
{
    int temp_a;
    temp_a = absolute_value(a);

    int temp_b;
    temp_b = absolute_value(b);

    while (temp_b != 0)
    {
        int remainder;
        remainder = temp_a;

        while (remainder >= temp_b)
        {
            remainder = remainder - temp_b;
        }

        temp_a = temp_b;
        temp_b = remainder;
    }
    return temp_a;
}

int test_harness(int limit, int target_gcd)
{
    int total_matches;
    total_matches = 0;

    int x;
    x = 1;

    while (x <= limit)
    {
        int y;
        y = 1;

        while (y <= limit)
        {
            int current_gcd;
            current_gcd = compute_gcd(x, y);

            if (current_gcd == target_gcd)
            {
                total_matches = total_matches + 1;
            }

            y = y + 1;
        }

        x = x + 1;
    }

    return total_matches;
}

int main()
{
    return test_harness(12, 3);
}

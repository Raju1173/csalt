int main()
{
    int x = foo(a, b) + c * (bar(d, e) - f) / baz(g, h);

    hello(x, foo(1, 2), bar(a + b, c));

    if(a > (b + c) * d)
    {
        doSomething();
    }

    while(a == false)
    {
        doSomething(a, b + c * d, e);
        a = true;
    }

    return x + y * z - w;
}

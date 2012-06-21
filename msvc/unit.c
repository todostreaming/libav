/*
 * Unit test for compound initializer C99-to-C89 replacement.
 */

typedef struct AVRational { int num, den; } AVRational;

AVRational call_function_2(AVRational x)
{
    x = (AVRational) { x.den, x.num };
    return (AVRational) { x.den, x.num };
}

int call_function(AVRational x)
{
    AVRational y = call_function_2(x);
    return x.num;
}

#define lut_vals(x) x, x+1, x+2, x+3
#define lut(x) { lut_vals(x), lut_vals(x+4) }
static const int l[][8] = { lut(0), lut(16), lut(32), lut(48) };

int main(int argc, char *argv[])
{
    int var;

    call_function((AVRational){1, 2});
    var = ((int[2]){1,2})[argc];
    var = call_function((AVRational){1, 2});
    if (var == 0) return call_function((AVRational){1, 2});
    else          return call_function((AVRational){2, 1});
}

// deadphi.cpp - branch folding in SSA form (Optimizer::simplifyCfg after toSsa).
// k is 2 (3) on both paths, which only the SSA phi shows: the second if folds there, and the phi of s
// must lose the operand of the arm that is gone, not keep it as a second copy from the entry block.

int deadArm(int x)                         // x + 1
{
    int k = 0;
    if (x > 0) k = 2; else k = 2;
    int s = 0;
    if (k == 2) s = x + 1; else s = x + 5;
    return s;
}

int deadArmConstant(int x, int y)          // 7
{
    int k = 0;
    if (x > y) k = 3; else k = 3;
    int s = 7;
    if (k != 3) s = x * y;
    return s;
}

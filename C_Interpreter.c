// 从0到1做一个C语言解释器

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

// to work with 64bit address
#define int long long

// current token
int token;         

// 主要在next中解析后返回值
int token_val;               

// pointer to source code string;
char *src, *old_src;          

// 池大小，数据文本堆栈的默认大小
int poolsize;      

 // line number
int line;          

int *text,                    // 代码段
    *old_text,                // 用于转存代码段
    *stack;                   // 栈

// 数据段
char *data;                   

//虚拟机寄存器
//pc：程序计数器，存放下一条要执行的计算机指令，在初始的时候应该指向main函数
//sp：指针寄存器，指向栈顶，并且栈是由高地址向低地址增长的，所以入栈时sp的值减小
//bp：基址指针，指向栈的某些位置，在调用函数的时候使用
//ax：通用寄存器，在虚拟机中存放一条指令执行的结果
int *pc, *bp, *sp, ax, cycle; 

int *current_id,              // 当前解析的ID
    *symbols;                 // 符号表
int *idmain;                  // the `main` function

//指令集，CPU能识别的命令集合，基于X86指令集，有参数的指令在前面，没有参数的指令在后面
enum
{
    LEA,
    IMM,
    JMP,
    CALL,
    JZ,
    JNZ,
    ENT,
    ADJ,
    LEV,
    LI,
    LC,
    SI,
    SC,
    PUSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    MSET,
    MCMP,
    EXIT
};

// tokens and classes (按优先级顺序排列，优先级大的在后面)
enum
{
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Else,
    Enum,
    If,
    Int,
    Return,
    Sizeof,
    While,
    Assign,
    Cond,
    Lor,
    Lan,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak
};

// 标识符字段
enum
{
    Token, //标识符返回的标记
    Hash,
    Name,
    Type,
    Class,
    Value,
    BType,
    BClass,
    BValue,
    IdSize
};

//变量及函数类型
enum
{
    CHAR,
    INT,
    PTR
};

// 声明类型
int basetype;  

// 表达式类型
int expr_type; 

// function frame
//
// 0: arg 1
// 1: arg 2
// 2: arg 3
// 3: return address
// 4: old bp pointer  <- index_of_bp
// 5: local var 1
// 6: local var 2

// index of bp pointer on stack
int index_of_bp; 

//词法分析，获取下一个标记
//自动忽略空白字符
void next()
{
    //令标识前进一个字符，识别完一个就返回
    //token=*src++; //注意此处等价于*(src++)即先前进一个字符再取其字符内容

    char *last_pos;
    int hash;

    while (token = *src)
    {
        ++src;

        // parse token here
        if (token == '\n')
        {
            ++line;
        }
        else if (token == '#')
        {
            // skip macro, because we will not support it
            while (*src != 0 && *src != '\n')
            {
                src++;
            }
        }
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_'))
        {

            // parse identifier
            last_pos = src - 1;
            hash = token;

            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_'))
            {
                hash = hash * 147 + *src;
                src++;
            }

            //寻找Token表里已存在的标识符，线性搜索
            current_id = symbols;
            while (current_id[Token])//如果当前标识符ID标记不为空
            {
                //memcmp()对前两个参数的前n个字节进行比较，此处是对前n个字节地址进行比较，Name存放着标识符本身
                if (current_id[Hash] == hash && !memcmp((char *)current_id[Name], last_pos, src - last_pos))
                {
                    //found one, return
                    token = current_id[Token];
                    return;
                }

                //继续搜索下一个已经存在的标识符
                current_id = current_id + IdSize;
            }

            // store new ID
            current_id[Name] = (int)last_pos;
            current_id[Hash] = hash;
            token = current_id[Token] = Id;
            return;
        }
        else if (token >= '0' && token <= '9')
        {
            // 解析数字, three kinds: dec(123) hex(0x123) oct(017)
            token_val = token - '0';
            if (token_val > 0)
            {
                // dec, starts with [1-9] 处理十进制
                while (*src >= '0' && *src <= '9')
                {
                    token_val = token_val * 10 + *src++ - '0';//进位
                }
            }
            else
            {
                // starts with 0  处理16进制及8进制
                if (*src == 'x' || *src == 'X')
                {
                    //hex
                    token = *++src;
                    while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F'))
                    {
                        token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                }
                else
                {
                    // oct
                    while (*src >= '0' && *src <= '7')
                    {
                        token_val = token_val * 8 + *src++ - '0';
                    }
                }
            }

            token = Num;
            return;
        }
        else if (token == '"' || token == '\'')
        {
            
            //解析字符串，当前唯一支持的转义字符是/n，可以继续扩充
            
            
            //store the string literal into data.
            //获取字符串存入的初始位置
            last_pos = data;
            while (*src != 0 && *src != token)
            {
                token_val = *src++;
                if (token_val == '\\')
                {
                    // 转义字符处理
                    token_val = *src++;
                    if (token_val == 'n')
                    {
                        token_val = '\n';
                    }
                }

                if (token == '"')// 注意token 和 token_val的区别
                {
                    // 每次循环时给数据集添加新字符
                    *data++ = token_val;
                }
            }

            src++;
            // if it is a single character, return Num token
            if (token == '"')
            {
                token_val = (int)last_pos;// 将数据集的地址转为int传给token_val
            }
            else
            {
                token = Num;
            }

            return;
        }
        else if (token == '/')
        {
            // 注释处理
            if (*src == '/')
            {
                // skip comments
                while (*src != 0 && *src != '\n')
                {
                    ++src;
                }
            }
            else
            {
                
                // divide operator
                // 注意循环开头的时候src++了一次，所以此处为第二个字符不为杠的情况
                token = Div;
                return;
            }
        }
        else if (token == '=')
        {
            // parse '==' and '='
            if (*src == '=')
            {
                src++;
                token = Eq;
            }
            else
            {
                token = Assign;
            }
            return;
        }
        else if (token == '+')
        {
            // parse '+' and '++'
            if (*src == '+')
            {
                src++;
                token = Inc;
            }
            else
            {
                token = Add;
            }
            return;
        }
        else if (token == '-')
        {
            // parse '-' and '--'
            if (*src == '-')
            {
                src++;
                token = Dec;
            }
            else
            {
                token = Sub;
            }
            return;
        }
        else if (token == '!')
        {
            // parse '!='
            if (*src == '=')
            {
                src++;
                token = Ne;
            }
            return;
        }
        else if (token == '<')
        {
            // parse '<=', '<<' or '<'
            if (*src == '=')
            {
                src++;
                token = Le;
            }
            else if (*src == '<')
            {
                src++;
                token = Shl;
            }
            else
            {
                token = Lt;
            }
            return;
        }
        else if (token == '>')
        {
            // parse '>=', '>>' or '>'
            if (*src == '=')
            {
                src++;
                token = Ge;
            }
            else if (*src == '>')
            {
                src++;
                token = Shr;
            }
            else
            {
                token = Gt;
            }
            return;
        }
        else if (token == '|')
        {
            // parse '|' or '||'
            if (*src == '|')
            {
                src++;
                token = Lor;
            }
            else
            {
                token = Or;
            }
            return;
        }
        else if (token == '&')
        {
            // parse '&' and '&&'
            if (*src == '&')
            {
                src++;
                token = Lan;
            }
            else
            {
                token = And;
            }
            return;
        }
        else if (token == '^')
        {
            token = Xor;
            return;
        }
        else if (token == '%')
        {
            token = Mod;
            return;
        }
        else if (token == '*')
        {
            token = Mul;
            return;
        }
        else if (token == '[')
        {
            token = Brak;
            return;
        }
        else if (token == '?')
        {
            token = Cond;
            return;
        }
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':')
        {
            // directly return the character as token;
            return;
        }
    }
    return;
}

//match调用next匹配完会返回token和token_val
void match(int tk)
{
    if (token == tk)
    {
        next();
    }
    else
    {
        printf("%d: expected token: %d\n", line, tk);
        exit(-1);
    }
}

//解析表达式
void expression(int level)
{
    // 表达式有多种格式
    // 但是主要可以分为两部分: unit and operator
    // for example `(char) *a[10] = (int *) func(b > 0 ? 10 : 20);
    // `a[10]` is an unit while `*` is an operator.
    // `func(...)` in total is an unit.
    // so we should first parse those unit and unary operators
    // and then the binary ones
    //
    // also the expression can be in the following types:
    //
    // 1. unit_unary ::= unit | unit unary_op | unary_op unit
    // 2. expr ::= unit_unary (bin_op unit_unary ...)

    // 一元运算符
    // unit_unary()
    int *id;
    int tmp;
    int *addr;
    {
        if (!token)
        {
            printf("%d: unexpected token EOF of expression\n", line);
            exit(-1);
        }

        // 数字常量加载到ax
        if (token == Num)
        { 
            match(Num);

            // emit code
            *++text = IMM;
            *++text = token_val; //token_val从next中返回
            expr_type = INT;
        }
        else if (token == '"')
        {
            // 连续字符串 "abc" "abc"

            // emit code
            *++text = IMM;
            *++text = token_val;

            match('"');

            // store the rest strings
            while (token == '"')
            {
                match('"');
            }

            // 数据向前移动一个位置，最后一个位置默认值为0
            // append the end of string character '\0', all the data are default
            // to 0, so just move data one position forward.
            data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
            expr_type = PTR;
        }
        else if (token == Sizeof)
        {
            // sizeof 实际上是一元运算符
            // now only `sizeof(int)`, `sizeof(char)` and `sizeof(*...)` are
            // supported.
            match(Sizeof);
            match('(');
            expr_type = INT;

            if (token == Int)
            {
                match(Int);
            }
            else if (token == Char)
            {
                match(Char);
                expr_type = CHAR;
            }

            while (token == Mul)
            {
                match(Mul);
                expr_type = expr_type + PTR;
            }

            match(')');

            // emit code
            *++text = IMM;
            *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

            expr_type = INT;
        }
        else if (token == Id)
        {
            // ID有几种类型
            // 但是这里是unit,所以只能是以下几种
            // 1. function call
            // 2. Enum variable
            // 3. global/local variable
            match(Id);

            id = current_id;

            if (token == '(')
            {
                // function call
                match('(');

                // pass in arguments
                tmp = 0; // number of arguments
                while (token != ')')
                {
                    expression(Assign);
                    *++text = PUSH;
                    tmp++;

                    if (token == ',')
                    {
                        match(',');
                    }
                }
                match(')');

                // emit code
                if (id[Class] == Sys)
                {
                    // system functions
                    *++text = id[Value];
                }
                else if (id[Class] == Fun)
                {
                    // function call
                    *++text = CALL;
                    *++text = id[Value];
                }
                else
                {
                    printf("%d: bad function call\n", line);
                    exit(-1);
                }

                // clean the stack for arguments
                if (tmp > 0)
                {
                    *++text = ADJ;
                    *++text = tmp;
                }
                expr_type = id[Type];
            }
            else if (id[Class] == Num)
            {
                // enum variable
                *++text = IMM;
                *++text = id[Value];
                expr_type = INT;
            }
            else
            {
                // variable
                if (id[Class] == Loc)
                {
                    *++text = LEA;
                    *++text = index_of_bp - id[Value];
                }
                else if (id[Class] == Glo)
                {
                    *++text = IMM;
                    *++text = id[Value];
                }
                else
                {
                    printf("%d: undefined variable\n", line);
                    exit(-1);
                }

                // emit code
                // 默认行为是加载地址中的值存储在ax中
                expr_type = id[Type];
                *++text = (expr_type == CHAR) ? LC : LI;
            }
        }
        else if (token == '(')
        {
            // 强制转换或表达式
            match('(');
            if (token == Int || token == Char)
            {
                tmp = (token == Char) ? CHAR : INT; // 转换类型
                match(token);
                while (token == Mul)
                {
                    match(Mul);
                    tmp = tmp + PTR;
                }

                match(')');

                expression(Inc); // cast 有同样的优先级 as Inc(++)

                expr_type = tmp;
            }
            else
            {
                // 普通括号
                expression(Assign);
                match(')');
            }
        }
        else if (token == Mul)
        {
            // dereference *<addr> 解引用
            match(Mul);
            expression(Inc); // dereference 有同样的优先级 as Inc(++)

            if (expr_type >= PTR)
            {
                expr_type = expr_type - PTR;
            }
            else
            {
                printf("%d: bad dereference\n", line);
                exit(-1);
            }

            *++text = (expr_type == CHAR) ? LC : LI;
        }
        else if (token == And)
        {
            //对于变量先加载地址，再根据类型使用LC/LI加载实际内容
            //IMM <addr>
            //LI

            //对变量取地址，只要不执行LC/LI即可，删除相应指令

            // get the address of
            match(And);
            expression(Inc); // get the address of
            if (*text == LC || *text == LI)
            {
                text--;
            }
            else
            {
                printf("%d: bad address of\n", line);
                exit(-1);
            }

            expr_type = expr_type + PTR;
        }
        else if (token == '!')
        {
            // 逻辑取反
            // not
            match('!');
            expression(Inc);

            // emit code, use <expr> == 0
            *++text = PUSH;
            *++text = IMM;
            *++text = 0;
            *++text = EQ;

            expr_type = INT;
        }
        else if (token == '~')
        {
            // 按位取反
            // bitwise not
            match('~');
            expression(Inc);

            // emit code, use <expr> XOR -1
            *++text = PUSH;
            *++text = IMM;
            *++text = -1;
            *++text = XOR;

            expr_type = INT;
        }
        else if (token == Add)
        {
            // +var, do nothing
            match(Add);
            expression(Inc);

            expr_type = INT;
        }
        else if (token == Sub)
        {
            // -var
            match(Sub);

            if (token == Num)
            {
                *++text = IMM;
                *++text = -token_val;
                match(Num);
            }
            else
            {

                *++text = IMM;
                *++text = -1;
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
            }

            expr_type = INT;
        }
        else if (token == Inc || token == Dec)
        {
            //自增自减
            tmp = token;
            match(token);
            expression(Inc);
            if (*text == LC)
            {
                *text = PUSH; // to duplicate the address
                *++text = LC;
            }
            else if (*text == LI)
            {
                *text = PUSH;
                *++text = LI;
            }
            else
            {
                printf("%d: bad lvalue of pre-increment\n", line);
                exit(-1);
            }
            *++text = PUSH;
            *++text = IMM;
            *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text = (tmp == Inc) ? ADD : SUB;
            *++text = (expr_type == CHAR) ? SC : SI;
        }
        else
        {
            printf("%d: bad expression\n", line);
            exit(-1);
        }
    }

    // 二元操作符和后缀操作符
    // binary operator and postfix operators.
    {
        while (token >= level)
        {
            // handle according to current operator's precedence
            tmp = expr_type;
            if (token == Assign)
            {
                // var = expr;
                match(Assign);
                if (*text == LC || *text == LI)
                {
                    *text = PUSH; // save the lvalue's pointer
                }
                else
                {
                    printf("%d: bad lvalue in assignment\n", line);
                    exit(-1);
                }
                expression(Assign);

                expr_type = tmp;
                *++text = (expr_type == CHAR) ? SC : SI;
            }
            else if (token == Cond)
            {
                // expr ? a : b;
                match(Cond);
                *++text = JZ;
                addr = ++text;
                expression(Assign);
                if (token == ':')
                {
                    match(':');
                }
                else
                {
                    printf("%d: missing colon in conditional\n", line);
                    exit(-1);
                }
                *addr = (int)(text + 3);
                *++text = JMP;
                addr = ++text;
                expression(Cond);
                *addr = (int)(text + 1);
            }
            else if (token == Lor)
            {
                // logic or
                match(Lor);
                *++text = JNZ;
                addr = ++text;
                expression(Lan);
                *addr = (int)(text + 1);
                expr_type = INT;
            }
            else if (token == Lan)
            {
                // logic and
                match(Lan);
                *++text = JZ;
                addr = ++text;
                expression(Or);
                *addr = (int)(text + 1);
                expr_type = INT;
            }
            else if (token == Or)
            {
                // bitwise or
                match(Or);
                *++text = PUSH;
                expression(Xor);
                *++text = OR;
                expr_type = INT;
            }
            else if (token == Xor)
            {
                // bitwise xor
                match(Xor);
                *++text = PUSH;
                expression(And);
                *++text = XOR;
                expr_type = INT;
            }
            else if (token == And)
            {
                // bitwise and
                match(And);
                *++text = PUSH;
                expression(Eq);
                *++text = AND;
                expr_type = INT;
            }
            else if (token == Eq)
            {
                // equal ==
                match(Eq);
                *++text = PUSH;
                expression(Ne);
                *++text = EQ;
                expr_type = INT;
            }
            else if (token == Ne)
            {
                // not equal !=
                match(Ne);
                *++text = PUSH;
                expression(Lt);
                *++text = NE;
                expr_type = INT;
            }
            else if (token == Lt)
            {
                // less than
                match(Lt);
                *++text = PUSH;
                expression(Shl);
                *++text = LT;
                expr_type = INT;
            }
            else if (token == Gt)
            {
                // greater than
                match(Gt);
                *++text = PUSH;
                expression(Shl);
                *++text = GT;
                expr_type = INT;
            }
            else if (token == Le)
            {
                // less than or equal to
                match(Le);
                *++text = PUSH;
                expression(Shl);
                *++text = LE;
                expr_type = INT;
            }
            else if (token == Ge)
            {
                // greater than or equal to
                match(Ge);
                *++text = PUSH;
                expression(Shl);
                *++text = GE;
                expr_type = INT;
            }
            else if (token == Shl)
            {
                // shift left
                match(Shl);
                *++text = PUSH;
                expression(Add);
                *++text = SHL;
                expr_type = INT;
            }
            else if (token == Shr)
            {
                // shift right
                match(Shr);
                *++text = PUSH;
                expression(Add);
                *++text = SHR;
                expr_type = INT;
            }
            else if (token == Add)
            {
                // add
                match(Add);
                *++text = PUSH;
                expression(Mul);

                expr_type = tmp;
                if (expr_type > PTR)
                {
                    // pointer type, and not `char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            }
            else if (token == Sub)
            {
                // sub
                match(Sub);
                *++text = PUSH;
                expression(Mul);
                if (tmp > PTR && tmp == expr_type)
                {
                    // pointer subtraction
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    expr_type = INT;
                }
                else if (tmp > PTR)
                {
                    // pointer movement
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    expr_type = tmp;
                }
                else
                {
                    // numeral subtraction
                    *++text = SUB;
                    expr_type = tmp;
                }
            }
            else if (token == Mul)
            {
                // multiply
                match(Mul);
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
                expr_type = tmp;
            }
            else if (token == Div)
            {
                // divide
                match(Div);
                *++text = PUSH;
                expression(Inc);
                *++text = DIV;
                expr_type = tmp;
            }
            else if (token == Mod)
            {
                // Modulo
                match(Mod);
                *++text = PUSH;
                expression(Inc);
                *++text = MOD;
                expr_type = tmp;
            }
            else if (token == Inc || token == Dec)
            {
                // postfix inc(++) and dec(--)
                // we will increase the value to the variable and decrease it
                // on `ax` to get its original value.
                if (*text == LI)
                {
                    *text = PUSH;
                    *++text = LI;
                }
                else if (*text == LC)
                {
                    *text = PUSH;
                    *++text = LC;
                }
                else
                {
                    printf("%d: bad value in increment\n", line);
                    exit(-1);
                }

                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? ADD : SUB;
                *++text = (expr_type == CHAR) ? SC : SI;
                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? SUB : ADD;
                match(token);
            }
            else if (token == Brak)
            {
                // array access var[xx]
                match(Brak);
                *++text = PUSH;
                expression(Assign);
                match(']');

                if (tmp > PTR)
                {
                    // pointer, `not char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                else if (tmp < PTR)
                {
                    printf("%d: pointer type expected\n", line);
                    exit(-1);
                }
                expr_type = tmp - PTR;
                *++text = ADD;
                *++text = (expr_type == CHAR) ? LC : LI;
            }
            else
            {
                printf("%d: compiler error, token = %d\n", line, token);
                exit(-1);
            }
        }
    }
}

void statement()
{
    // there are 6 kinds of statements here:
    // 1. if (...) <statement> [else <statement>]
    // 2. while (...) <statement>
    // 3. { <statement> }
    // 4. return xxx;
    // 5. <empty statement>;
    // 6. expression; (expression end with semicolon)

    int *a, *b; // 位置跳转

    if (token == If)
    {
        // if (...) <statement> [else <statement>]
        //
        //   if (...)           <cond>
        //                      JZ a
        //     <statement>      <statement>
        //   else:              JMP b
        // a:                 a:
        //     <statement>      <statement>
        // b:                 b:
        //
        //

        // 解释：
        //     如果条件失败，则跳转到a执行false
            
        //     如果条件成功，则不跳转，顺序执行true，
        //     执行完跳转到b略过false

        match(If);
        match('(');
        expression(Assign); // parse condition
        match(')');

        // emit code for if

        // JZ：条件为0即失败的时候跳转
        *++text = JZ; 

        // 标记跳转位置b
        b = ++text;

        // 递归调用再次解析if内语句
        statement();

        if (token == Else)
        { 
            // parse else
            match(Else);

            //对跳转位置赋值，这里写的有点抽象
            //实际上是忽略下面两个语句，将跳转位置设置在两条语句之后，即false_statement之前
            *b = (int)(text + 3);

            //跳转到最后略过false语句,即略过下方语句
            *++text = JMP;

            //标记跳转位置
            b = ++text;

            statement();
        }

        //在false_statement进行之前标记的跳转地址赋值
        *b = (int)(text + 1);
    }
    else if (token == While)
    {
        //
        // a:                     a:
        //    while (<cond>)        <cond>
        //                          JZ b
        //     <statement>          <statement>
        //                          JMP a
        // b:                     b:

        //如果条件不成立，跳转到b即跳出

        match(While);

        //标记起始地址
        a = text + 1;

        match('(');
        expression(Assign);
        match(')');

        *++text = JZ;

        //标记跳转位置b
        b = ++text;

        statement();

        *++text = JMP;
        *++text = (int)a;

        //对之前标记的位置赋值：文本最末端地址
        *b = (int)(text + 1);
    }
    else if (token == '{')
    {
        // { <statement> ... }
        match('{');

        while (token != '}')
        {
            statement();
        }

        match('}');
    }
    else if (token == Return)
    {
        // return [expression];
        match(Return);

        if (token != ';')
        {
            expression(Assign);
        }

        match(';');

        //添加返回汇编代码
        *++text = LEV;
    }
    else if (token == ';')
    {
        // empty statement
        match(';');
    }
    else
    {
        // a = b; or function_call();
        expression(Assign);
        match(';');
    }
}

void function_parameter()
{
    int type;
    int params;
    params = 0;
    while (token != ')')
    {
        // int name, ...
        type = INT;
        if (token == Int)
        {
            match(Int);
        }
        else if (token == Char)
        {
            type = CHAR;
            match(Char);
        }

        // pointer type
        while (token == Mul)
        {
            match(Mul);
            type = type + PTR;//根据有几个*确定指针的类型标记
        }

        // parameter name
        if (token != Id)
        {
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        if (current_id[Class] == Loc)
        {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }

        match(Id);

        //先将全部变量信息保存，再存储局部变量
        current_id[BClass] = current_id[Class];
        current_id[Class] = Loc;
        current_id[BType] = current_id[Type];
        current_id[Type] = type;
        current_id[BValue] = current_id[Value];

        // index of current parameter
        current_id[Value] = params++; 

        if (token == ',')
        {
            match(',');
        }
    }
    index_of_bp = params + 1;
}

void function_body()
{
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    //要求变量定义出现在所有语句之前

    int pos_local; //局部变量在栈上的位置
    int type;
    pos_local = index_of_bp;

    while (token == Int || token == Char)
    {
        //局部变量声明
        basetype = (token == Int) ? INT : CHAR;
        match(token);

        while (token != ';')
        {
            type = basetype;
            while (token == Mul)
            {
                match(Mul);
                type = type + PTR;
            }

            if (token != Id)
            {
                // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }
            if (current_id[Class] == Loc)
            {
                // identifier exists
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }
            match(Id);

            // store the local variable
            current_id[BClass] = current_id[Class];
            current_id[Class] = Loc;
            current_id[BType] = current_id[Type];
            current_id[Type] = type;
            current_id[BValue] = current_id[Value];
            current_id[Value] = ++pos_local; // index of current parameter

            if (token == ',')
            {
                match(',');
            }
        }
        match(';');
    }

    //优先级相同时，自右向左执行
    //保存局部变量的栈大小,预留空间
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    // statements处理
    while (token != '}')
    {
        statement();
    }

    //添加离开子函数的代码
    *++text = LEV;
}

void function_declaration()
{
    // type func_name (...) {...}
    //               | this part

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    //match('}');在global_declaration中解析

    //将符号表中的信息恢复成全局信息
    //因为上面function_body()里局部变量覆盖掉了全局变量
    current_id = symbols;
    while (current_id[Token])
    {
        if (current_id[Class] == Loc)
        {
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] = current_id[BValue];
        }
        current_id = current_id + IdSize;
    }
}

void enum_declaration()
{
    // parse enum [id] { a = 1, b = 3, ...}
    int i;
    i = 0;
    while (token != '}')
    {
        if (token != Id)
        {
            printf("%d: bad enum identifier %d\n", line, token);
            exit(-1);
        }
        next();
        if (token == Assign)
        {
            // like {a=10}
            next();
            if (token != Num)
            {
                printf("%d: bad enum initializer\n", line);
                exit(-1);
            }
            i = token_val;//获取token值
            next();
        }

        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;

        if (token == ',')
        {
            next();
        }
    }
}

void global_declaration()
{
        // EBNF 声明方式，自顶向下逐步解析（递归下降），此处只支持4种

        // program ::= {global_declaration}+

        // global_declaration ::= enum_decl | variable_decl | function_decl

        // enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'

        // variable_decl ::= type {'*'} id { ',' {'*'} id } ';'

        // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

        // parameter_decl ::= type {'*'} id {',' type {'*'} id}

        // body_decl ::= {variable_decl}, {statement}

        // statement ::= non_empty_statement | empty_statement

        // non_empty_statement ::= if_statement | while_statement | '{' statement '}'
        // | 'return' expression | expression ';'

        // if_statement ::= 'if' '(' expression ')' statement ['else' non_empty_statement]

        // while_statement ::= 'while' '(' expression ')' non_empty_statement

    int type; // 变量的实际类型
    int i;    // tmp

    basetype = INT;

    // 解析enum, this should be treated alone.
    if (token == Enum)
    {
        // enum [id] { a = 10, b = 20, ... }
        match(Enum);
        if (token != '{')
        {
            // skip the [id] part
            match(Id); 
        }

         //解析赋值部分
        if (token == '{')
        {
            // parse the assign part
            match('{');
            enum_declaration();
            match('}');
        }

        match(';');
        return;
    }

    // parse type information
    if (token == Int)
    {
        match(Int);
    }
    else if (token == Char)
    {
        match(Char);
        basetype = CHAR;
    }

    //解析以逗号分离的变量声明
    while (token != ';' && token != '}')
    {
        type = basetype;

        //解析指针类型，可能存在这样的：int ****x；声明
        while (token == Mul)
        {
            match(Mul);

            //如果是指针，在类型里加上PTR
            //如果是指针的指针，则再加上PTR
            type = type + PTR;
        }

        if (token != Id)
        {
            //无效的声明
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
        if (current_id[Class])
        {
            //标识符已存在
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        if (token == '(')
        {
            current_id[Class] = Fun;
            current_id[Value] = (int)(text + 1); // 函数的内存地址
            function_declaration();
        }
        else
        {
            // variable declaration
            current_id[Class] = Glo;       // global variable
            current_id[Value] = (int)data; // assign memory address
            data = data + sizeof(int);

            //解析到类型需要lookhead来查看解析到的是变量还是函数，如果遇到(
            //则断定为函数，反之则是变量
        }

        if (token == ',')
        {
            match(',');
        }
    }
    next();
}

//词法分析的入口，分析整个C语言程序
void program()
{
    // get next token
    next();

    //标识字符，当标识字符大于0时（等于0为空格NULL）
    while (token > 0)
    {
        //printf("token is: %c\n", token);
        //next();
        global_declaration();
    }
}

//虚拟机入口，解释目标代码
int eval()
{

    // IMM &lt;num&gt; 将 &lt;num&gt; 放入寄存器 ax 中。
    // LC 将对应地址中的字符载入 ax 中，要求 ax 中存放地址。
    // LI 将对应地址中的整数载入 ax 中，要求 ax 中存放地址。
    // SC 将 ax 中的数据作为字符存放入地址中，要求栈顶存放地址。
    // SI 将 ax 中的数据作为整数存放入地址中，要求栈顶存放地址。
    // PUSH 将 ax 的值压入栈中
    // 注意：
    // 1，++比*优先级高
    // 2，*sp++的作用是退栈，相当于pop()
    // 3，(char *)是字符地址
    // 注解：LC（取）/SC（存） 和 LI/SI 就是对应字符型和整型的存取操作

    // 此处还有一个return的RET，但是可以用LEV代替
    // else if(op==RET){
    //     pc=(int *)*sp++;
    // }

    int op, *tmp;
    while (1)
    {
        op = *pc++; // get next operation code

        if (op == IMM)
        {
            ax = *pc++;
        } // load immediate value to ax
        else if (op == LC)
        {
            ax = *(char *)ax;
        } // load character to ax, address in ax
        else if (op == LI)
        {
            ax = *(int *)ax;
        } // load integer to ax, address in ax
        else if (op == SC)
        {
            ax = *(char *)*sp++ = ax;
        } // save character to address, value in ax, address on stack
        else if (op == SI)
        {
            *(int *)*sp++ = ax;
        } // save integer to address, value in ax, address on stack
        else if (op == PUSH)
        {
            *--sp = ax;
        } // push the value of ax onto the stack
        else if (op == JMP)
        {
            //当前指令存放着下一个要跳转的地址
            pc = (int *)*pc;
        } // jump to the address
        else if (op == JZ)
        {
            //如果ax为0的话跳转
            pc = ax ? pc + 1 : (int *)*pc;
        } // jump if ax is zero
        else if (op == JNZ)
        {
            //如果ax不为0的话跳转
            pc = ax ? (int *)*pc : pc + 1;
        } // jump if ax is not zero
        else if (op == CALL)
        {
            //先将跳转子函数的位置存储在栈中
            *--sp = (int)(pc + 1);

            //然后再跳转到子函数
            pc = (int *)*pc;
        } // call subroutine
        //else if (op == RET)  {pc = (int *)*sp++;}                              // return from subroutine;
        else if (op == ENT)
        {
            *--sp = (int)bp;

            //保存当前栈指针
            bp = sp;

            //为局部变量留出空间
            sp = sp - *pc++;
        } // make new stack frame
        else if (op == ADJ)
        {
            sp = sp + *pc++;
        } // add esp, <size>
        else if (op == LEV)
        {
            sp = bp;
            bp = (int *)*sp++;
            pc = (int *)*sp++;
        } // restore call frame and PC
        else if (op == ENT)
        {
            *--sp = (int)bp;
            bp = sp;
            sp = sp - *pc++;
        } // make new stack frame
        else if (op == ADJ)
        {
            //在调用子函数的时候将压入栈中的数据清除
            sp = sp + *pc++;
        } // add esp, <size>
        else if (op == LEV)
        {
            sp = bp;
            bp = (int *)*sp++;

            //重新回到调用子函数的位置
            pc = (int *)*sp++;
        } // restore call frame and PC
        else if (op == LEA)
        {
            //参数的加载地址
            ax = (int)(bp + *pc++);
        } // load address for arguments.

        else if (op == OR)
            ax = *sp++ | ax;
        else if (op == XOR)
            ax = *sp++ ^ ax;
        else if (op == AND)
            ax = *sp++ & ax;
        else if (op == EQ)
            ax = *sp++ == ax;
        else if (op == NE)
            ax = *sp++ != ax;
        else if (op == LT)
            ax = *sp++ < ax;
        else if (op == LE)
            ax = *sp++ <= ax;
        else if (op == GT)
            ax = *sp++ > ax;
        else if (op == GE)
            ax = *sp++ >= ax;
        else if (op == SHL)
            ax = *sp++ << ax;
        else if (op == SHR)
            ax = *sp++ >> ax;
        else if (op == ADD)
            ax = *sp++ + ax;
        else if (op == SUB)
            ax = *sp++ - ax;
        else if (op == MUL)
            ax = *sp++ * ax;
        else if (op == DIV)
            ax = *sp++ / ax;
        else if (op == MOD)
            ax = *sp++ % ax;

        else if (op == EXIT)
        {
            printf("exit(%d)", *sp);
            return *sp;
        }
        else if (op == OPEN)
        {
            ax = open((char *)sp[1], sp[0]);
        }
        else if (op == CLOS)
        {
            ax = close(*sp);
        }
        else if (op == READ)
        {
            ax = read(sp[2], (char *)sp[1], *sp);
        }
        else if (op == PRTF)
        {
            tmp = sp + pc[1];
            ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
        }
        else if (op == MALC)
        {
            ax = (int)malloc(*sp);
        }
        else if (op == MSET)
        {
            ax = (int)memset((char *)sp[2], sp[1], *sp);
        }
        else if (op == MCMP)
        {
            ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
        }
        else
        {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
        //后面函数的功能实现太复杂，直接调用C语言内部的函数支持
    }
    return 0;
}

#undef int // Mac/clang needs this to compile

//main函数的argc 和 argv 参数是操作系统传入的
//argc：传入参数的个数+1
//argv：存放传给main函数的参数，其中argv[0]存放可执行程序的文件名字
int main(int argc, char **argv)
{
#define int long long // to work with 64bit address

    int i, fd;
    int *tmp;

    argc--;//参数-1
    argv++;//数组指针+1

    poolsize = 256 * 1024; // arbitrary size
    line = 1;

    //open返回文件句柄（文件路径，访问模式 0为只读模式）
    if ((fd = open(*argv, 0)) < 0)
    {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    //为虚拟机分配内存
    if (!(text = old_text = malloc(poolsize)))
    {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = malloc(poolsize)))
    {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = malloc(poolsize)))
    {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    if (!(symbols = malloc(poolsize)))
    {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }

    //将代码段用0初始化
    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);

    //初始化寄存器地址=栈位置+池大小
    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;

    // 虚拟机测试代码片段
    // i = 0;
    // text[i++] = IMM;
    // text[i++] = 10;
    // text[i++] = PUSH;
    // text[i++] = IMM;
    // text[i++] = 20;
    // text[i++] = ADD;
    // text[i++] = PUSH;
    // text[i++] = EXIT;
    // pc = text;

    //初始化关键字与内置函数，先对这些文本进行分析一遍添加到关键字
    src = "char else enum if int return sizeof while "
          "open read close printf malloc memset memcmp exit void main";

    //添加关键字到标识表
    i = Char;
    while (i <= While)
    {
        next();
        current_id[Token] = i++;
    }

    // add library to symbol table
    i = OPEN;
    while (i <= EXIT)
    {
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    next();
    current_id[Token] = Char; // handle void type

    
    next();
    idmain = current_id; // keep track of main
    //需要再研究current_id是怎么运行的

    // read the source file
    if ((fd = open(*argv, 0)) < 0)
    {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    //分配池内存返回开头指针给src和old_src
    if (!(src = old_src = malloc(poolsize)))
    {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }

    //读取源文件(read返回值0代表EOF，-1代表出错)
    //read(文件句柄，缓存，n字节)读取n字节到缓存中
    if ((i = read(fd, src, poolsize - 1)) <= 0)
    {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0; // 添加EOF到末尾
    close(fd);

    program();

    if (!(pc = (int *)idmain[Value]))
    {
        printf("main() not defined\n");
        return -1;
    }

    // setup stack
    sp = (int *)((int)stack + poolsize);
    *--sp = EXIT; // call exit if main returns
    *--sp = PUSH;
    tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;

    return eval();
}
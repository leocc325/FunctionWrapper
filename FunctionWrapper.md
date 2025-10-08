# 这是一个FunctionWrapper.hpp的使用说明
这里是两个目标函数用于测试
```c++
double add(int a,double b)
{
    std::cout<<__PRETTY_FUNCTION__<<a<<" "<<b<<std::endl<<std::flush;
    return a + b;
}

class Object
{
public:
    double subtract(double a,int b)
    {
        std::cout<<__PRETTY_FUNCTION__<<a<<" "<<b<<std::endl<<std::flush;
        return a - b;
    }
};
```

## 一：关于类FunctionWrapper的成员函数和使用方法介绍


FunctionWrapper是一个函数包装器,可以将其视为一个***完全擦除了类型的std::function***,但是在类的内部保存了所有必要的信息,这些信息如下：<br />
***函数指针、函数指针信息、函数参数信息、参数包指针、参数包信息、返回值指针、返回值信息***

### FunctionWrapper成员函数简介:<br />

#### 1.默认构造函数FunctionWrapper()
生成一个不包含函数指针和参数指针以及信息的的FunctionWrapper,这也意味着后续无法再设置其函数指针(移动构造和赋值除外) <br />
```c++
FunctionWrapper funcA;
//创建了一个不包含函数指针和参数包的FunctionWrapper;
```

#### 2.构造函数template<typename Func,typename Obj> FunctionWrapper(Func func,Obj* obj = nullptr)
这个构造函数接受一个函数指针作为参数,函数、参数等信息会在这个构造函数中进行初始化。<br />
```c++
FunctionWrapper funcB(add);
//创建了一个可以调用自由函数add(int,double)的FunctionWrapper

Object* obj = new Object();
FunctionWrapper funcC(&Object::subtract,obj);
//创建了一个可以调用成员函数Object::subtract的FunctionWrapper
```

### 3.template<typename...Args> setArgs(Args&&...args)
设置函数调用需要的参数,由于FunctionWrapper内部擦除了函数参数的类型信息(并非完全擦除,只是无法在这里获取,这也是FunctionWrapper的一个不完美的地方,但是确实想不到更好的解决办法了),所以为了确保后续对函数指针调用不发生异常,这里对传入参数的类型进行了非常严格的检查,不会对参数进行隐式转换,需要确保传入的每一个参数的类型和函数指针的参数完全一致,否则抛出异常。如果参数包为空,则setArgs函数内部什么都不会做。
```c++
//以funcB为例,内部的函数指针需要的参数类型分别为int,double

funcB.setArgs(5,10);
//10会被推导为int类型,和第二个参数double类型不符合,抛出异常

funcB.setArgs(5,double(10));
//两个参数类型都完全匹配,正确
```

#### 4.void exec()
exec()为调用保存的函数指针的最终接口,这个函数内部会将保存的参数转发到保存的函数指针并完成函数指针的调用。
如果FunctionWrapper内部没有包含函数指针,则什么都不会发生并且打印错误信息 ***函数指针信息 + :error:functor is empty,excute failed***
如果FunctionWrapper内部设置函数参数,同样什么都不会发生并且打印错误信息 ***函数指针信息 + :error:no parameter has been setted,excute failed***
```c++
//对于以下代码,执行之后会产生输出
FunctionWrapper funcA;

FunctionWrapper funcB(add);
    
Object* obj = new Object();
FunctionWrapper funcC(&Object::subtract,obj);
    
funcB.exec();
//此时funcB内部保存的参数为空,打印信息为:PFdidE  error:no parameter has been setted,excute failed
    
funcB.setArgs(5,double(10));
//将funcB的参数设置为5,10

funcB.exec();
//再次执行funcB,打印信息为:double add(int, double)5 10,表明add已经被成功调用
    
funcB.setArgs(10,double(20));
//重新将funcB的参数设置为10,20

funcB.exec();
//再次执行funcB,打印信息为:double add(int, double)10 20,funcB参数更新成功

funcC.exec();
//打印信息为:M6ObjectFddiE  error:no parameter has been setted,excute failed
    
funcC.setArgs(2.5,1);
//将Object::subtract的参数设置为2.5,1
    
funcC.exec();
//执行funcC,打印信息为:double Object::subtract(double, int)2.5 1
    
funcC.setArgs(double(30),10);
//将funcC参数设置为30,10
    
funcC.exec();
//执行funcC,打印信息为:double Object::subtract(double, int)30 10
    
```
调用setArgs并且参数设置成功之后,这些参数将长期保存在FunctionWrapper内部,不会在exec执行之后被清空。这些参数将被反复使用,直到下一次调用setArgs刷新保存的参数。

#### 5.template<typename...Args> void exec(Args&&...args)
exec()的重载,含义为使用Args作为参数并调用内部函数指针
```c++
funcC.setArgs(2.5,1); 
funcC.exec();

//上面两行代码完全等价于下面这一行代码

funcC.exec(2.5,1);
```
总的来说,这两种方案的区别仅仅在与setArgs提供了一种可以延时执行函数的机制,比如在保存历史状态、撤销、重做等情况下需要保存函数参数但是并不需要马上执行。exec(Args&&...args)则代表了对函数的立刻执行。

#### 6.template<typename...Args> void operator() (Args&&...args)
完全等同于exec(Args&&...args)

#### 7.template<typename RT> getResult()
获取返回值,需要指定完全正确的返回值类型(不支持隐式转换),否则会抛出异常
```c++
funcC.exec(22.123,10);
float ret1 = funcC.getResult<float>();
//ret1抛出异常,因为funcC内部的函数指针返回值为double,但是在获取返回值时指定的类型为float,导致转换失败
double ret2 = funcC.getResult<double>();
//ret2 = 12.123
```

#### 8.void getResult(void* ptr)
获取返回值,不如template<typename RT> getResult()好用
```c++
double ret3 = 0;
funcC.getResult(&ret3);
//ret3 = 12.123
```

### FunctionWrapper额外成员函数补充,这一部分功能需要将CONSOLECALL设置为1并且包含StringConvertorQ.hpp才可正常使用<br />
这些额外的函数支持将字符串参数转换为函数变量并完成对函数的调用,可以用于控制台、远程指令等功能。

#### 9.void setStringArgs(const QStringList& args)
将QStringList切割为若干个字符串,并且将字符串转换为对应的参数保存在FunctionWrapper内
```c++
QStringList args = {"60","30"};
funcC.setStringArgs(args);

funcC.exec();
//打印信息为:double Object::subtract(double, int)60 30
```

#### 10. void execString(const QStringList& strList)
将字符串作为参数传入并且调用函数
```c++
QStringList args = {"60","30"};

funcC.setStringArgs(args);
funcC.exec();

//上面两行代码完全等价于下面这一行代码

funcC.execString(args);
```

#### 11.QString getResultString() const noexcept
获取返回值并将返回值转换为字符串

## 二：一个完整的示例。

```c++
inline void funcwrapperTest()
{
    FunctionWrapper funcA;//创建一个空的函数包装器
    funcA.exec();//输出:Dn  error:functor is empty,excute failed

    FunctionWrapper funcB(add);//创建一个add(double,int)的函数包装器,但是不指定初始参数
    funcB.exec();//输出:PFdidE  error:no parameter has been setted,excute failed

    funcB.setArgs(10,10.5);//将funcB的函数参数设置为10和10.5
    funcB.exec();//输出:double add(int, double)10 10.5

    funcA = funcB;//将funcB的状态赋值给空包装器funcA   ##1
    funcB.exec(6,1.25);//重新设置funcB的参数并执行,输出:double add(int, double)6 1.25
    funcA.exec();//执行funcA,输出:double add(int, double)10 10.5  ,说明此时funcA的状态等价于##1时funcB的状态

    funcA.exec(20,1.5);//重新设置funcA的参数并执行,输出:double add(int, double)20 1.5
    funcA.exec();//再次执行funcA,由于没有重新设置参数,所以输出依然为:double add(int, double)20 1.5

    //创建一个成员函数的包装器,但是不指定初始函数参数
    Object obj;
    FunctionWrapper funcC(&Object::subtract,&obj);

    funcC.exec();//未设置参数,所以输出:error:no parameter has been setted,excute failed
    try {
        funcC.exec(10,10); //参数类型不匹配,抛出异常,这里参数被推导为int,int  但是函数所需参数为double,int
    } catch (std::exception& e){
        std::cout<<e.what()<<std::endl<<std::flush;    //输出:error:Argument type or number mismatch
    }
    funcC.exec(double(10),10);//显示指定参数类型并执行,输出:double Object::subtract(double, int)10 10

    double retsult = funcC.getResult<double>();//result = 10;

    //再次尝试获取返回值,但是指定一个错误类型
    try {
        retsult = funcC.getResult<float>();
    } catch (std::exception& e){
        std::cout<<e.what()<<std::endl<<std::flush;    //输出:error:return value type mismatch
    }
}
```

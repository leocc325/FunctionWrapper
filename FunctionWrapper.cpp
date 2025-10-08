#include "FunctionWrapper.hpp"

FunctionWrapper::FunctionPrivate::FunctionPrivate()
{
    using Ret = void;
    using ArgsTuple = std::tuple<>;

    //默认初始化参数类型和返回值类型为空的tuple和void
    argTupleInfo = &typeid(ArgsTuple);
    resultInfo = &typeid(Ret);
    funcInfo = &typeid (std::nullptr_t);
}

FunctionWrapper::FunctionPrivate::~FunctionPrivate()
{
    if(deleteHelper)
        deleteHelper(argsTuple,result);
}

FunctionWrapper::FunctionPrivate::FunctionPrivate(const FunctionWrapper::FunctionPrivate &other)
{
    this->copyImpl(other);
}

FunctionWrapper::FunctionPrivate::FunctionPrivate(FunctionWrapper::FunctionPrivate &&other) noexcept
{
    this->moveImpl(std::move(other));
}

FunctionWrapper::FunctionPrivate &FunctionWrapper::FunctionPrivate::operator =(const FunctionWrapper::FunctionPrivate &other)
{
    //使用赋值运算符时需要先判断两个对象内部数据(返回值和参数包)类型是否一致,不一致的情况下需要先回收被赋值对象的内存，再重新分配内存并赋值
    if(*this != other && deleteHelper)
        deleteHelper(this->result,this->argsTuple);

    this->copyImpl(other);
    return *this;
}

FunctionWrapper::FunctionPrivate &FunctionWrapper::FunctionPrivate::operator =(FunctionWrapper::FunctionPrivate &&other) noexcept
{
    this->moveImpl(std::move(other));
    return *this;
}

bool FunctionWrapper::FunctionPrivate::operator ==(const FunctionWrapper::FunctionPrivate &other)
{
    //返回值和参数列表一样就认为这两个对象相等,因为他们保存返回值和参数的数据内存类型是一样的
    return (*this->resultInfo == *other.resultInfo) && (*this->argTupleInfo == *other.argTupleInfo);
}

bool FunctionWrapper::FunctionPrivate::operator !=(const FunctionWrapper::FunctionPrivate &other)
{
    return !operator==(other);
}

void FunctionWrapper::FunctionPrivate::copyImpl(const FunctionPrivate& other)
{
    this->functor = other.functor;
    this->argsHelper = other.argsHelper;
    this->stringArgsHelper = other.stringArgsHelper;
    this->deleteHelper = other.deleteHelper;
    this->resultHelper = other.resultHelper;
    this->resultString = other.resultString;

    this->resultInfo = other.resultInfo;
    this->argTupleInfo = other.argTupleInfo;
    this->funcInfo = other.funcInfo;

    this->argsHelper(other.argsTuple,this->argsTuple);
    this->resultHelper(other.result,this->result);
}

void FunctionWrapper::FunctionPrivate::moveImpl(FunctionWrapper::FunctionPrivate&& other) noexcept
{
    this->functor = std::move(other.functor);

    this->argsHelper = other.argsHelper;
    other.argsHelper = nullptr;

    this->stringArgsHelper = other.stringArgsHelper;
    other.stringArgsHelper = nullptr;

    this->deleteHelper = other.deleteHelper;
    other.deleteHelper = nullptr;

    this->resultHelper = other.resultHelper;
    other.resultHelper = nullptr;

    this->resultString = std::move(other.resultString);

    this->resultInfo = other.resultInfo;
    other.resultInfo = nullptr;

    this->argTupleInfo = other.argTupleInfo;
    other.argTupleInfo = nullptr;

    this->funcInfo = other.funcInfo;
    other.funcInfo = nullptr;

    this->argsTuple = other.argsTuple;
    other.argsTuple = nullptr;

    this->result = other.result;
    other.result = nullptr;
}

FunctionWrapper::FunctionWrapper()
{
    d = new FunctionPrivate();
}

FunctionWrapper::FunctionWrapper(const FunctionWrapper &other)
{
    d = new FunctionPrivate(*other.d);
}

FunctionWrapper::FunctionWrapper(FunctionWrapper &&other) noexcept
{
    *this->d = std::move(*other.d);
}

FunctionWrapper &FunctionWrapper::operator =(const FunctionWrapper& other)
{
    *this->d = *other.d;
    return *this;
}

FunctionWrapper &FunctionWrapper::operator =(FunctionWrapper&& other) noexcept
{
    *this->d = std::move(*other.d);
    return *this;
}

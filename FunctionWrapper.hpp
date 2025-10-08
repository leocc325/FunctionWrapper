#ifndef FunctionWrapper_H
#define FunctionWrapper_H

#define CONSOLECALL 0

#include "FunctionTraits.hpp"
#if CONSOLECALL
#include "StringConvertorQ.hpp"
#endif
#include <stdexcept>
#include <functional>
#include <future>
#include <QDebug>

//使用消息id获取返回值类型
template <std::size_t N>
struct FunctionRT{using type = void;};

template <>
struct FunctionRT<std::numeric_limits<std::size_t>::max()>{using type = void;};

//在excel中填写函数的返回值类型，然后用python脚本自动生成FunctionRT模板用来获取函数的返回值
#define megRegister(msg, func) \
template <> \
struct FunctionRT<msg>{using type = func;};

using namespace MetaUtility;

class FunctionWrapper
{
#if CONSOLECALL
    template<int Index, typename Tuple>
    struct TupleHelper;

    template<typename Tuple>
    struct TupleHelper<-1, Tuple>
    {
        static void set(Tuple& , const QStringList& ) {}
    };

    template<int Index, typename Tuple>
    struct TupleHelper
    {
        static void set(Tuple& tpl, const QStringList& strList)
        {
            if  (Index < strList.size())
            {
                convertStringToArg(strList.at(Index),std::get<Index>(tpl));
                TupleHelper<Index - 1, Tuple>::set(tpl, strList);
            }
            else
            {
                throw std::out_of_range("Index out of range for QStringList");
            }
        }
    };
#endif

#if 0
    //直接传递函数指针类型作为模板参数会导致模板膨胀
    //两个返回值一样、参数数量类型一样的的函数指针也会实例化两套模板
    //所以推荐使用以返回值、函数参数元组作为模板参数
    template<typename Func>
    struct Manager
    {
        using Ret = typename FunctionTraits<Func>::ReturnType;
        using ArgsTuple = typename FunctionTraits<Func>::BareTupleType;
        static constexpr unsigned arity = FunctionTraits<Func>::Arity;
#else
    template<typename Ret,typename ArgsTuple,unsigned Arity = std::tuple_size<ArgsTuple>::value>
    struct Manager
    {
#endif
        static void setArgs(void* from,void*& to)
        {
            //需要在外面确保两个指针的类型一致,否则会UB
            if(from == nullptr)
                return;

            initTuple(to);
            *static_cast<ArgsTuple*>(to) =  *static_cast<ArgsTuple*>(from);
        }

        static void setStringArgs(const QStringList& argsList,void*& argsTuple)
        {
#if CONSOLECALL
            if(Arity != argsList.count()){
                qCritical()<<QString("error:argments number mismatch,%1/%2").arg(Arity).arg(argsList.count());
                return;
            }

            initTuple(argsTuple);
            TupleHelper<Arity - 1, ArgsTuple>::set(*static_cast<ArgsTuple*>(argsTuple), argsList);
#endif
        }

        static void deleteMembers(void* args,void* ret)
        {
            deleter(static_cast<ArgsTuple*>(args));
            deleter(static_cast<Ret*>(ret));
        }

        static void initMembers(void*& args,const std::type_info*& argsInfo,void*& ret,const std::type_info*& retInfo)
        {
            init<ArgsTuple>(args,argsInfo);
            init<Ret>(ret,retInfo);
        }

        template<typename T> static typename std::enable_if<!std::is_void<T>::value>::type
        deleter(T* ptr)
        {
            if(ptr != nullptr)
            {
                ptr->~T();
                ::operator delete(ptr);
                ptr = nullptr;
            }
        }

        template<typename T> static typename std::enable_if<std::is_void<T>::value>::type
        deleter(T* ptr)
        {
            if(ptr != nullptr)
            {
                ::operator delete(ptr);
                ptr = nullptr;
            }
        }

        template<typename T> static typename std::enable_if<!std::is_void<T>::value>::type
        init(void*& ptr,const std::type_info*& info)
        {
            if(ptr == nullptr)
            {
                ptr = static_cast<T*>(::operator new(sizeof (T)));
                info = &typeid(T);
            }
        }

        template<typename T> static typename std::enable_if<std::is_void<T>::value>::type
        init(void*& ptr,const std::type_info*& info)
        {
            if(ptr == nullptr)
            {
                info = &typeid(T);
            }
        }

        template<typename T = Ret>
        static typename std::enable_if<!std::is_void<T>::value>::type
        result(void* from,void*& to)
        {
            if(from != nullptr)
            {
                if(to == nullptr)
                {
                    to = static_cast<T*>(new T());
                }
                *static_cast<Ret*>(to) = *static_cast<Ret*>(from);
            }
        }

        template<typename T = Ret>
        static typename std::enable_if<std::is_void<T>::value>::type
        result(void* ,void*& )
        {

        }

        static void initTuple(void*& tpl)
        {
            if(tpl == nullptr)
            {
                tpl = new ArgsTuple();
            }
            else
            {
                static_cast<ArgsTuple*>(tpl)->~ArgsTuple();
            }
        }
    };

    class FunctionPrivate
    {
        //***现在测试暂时使用new分配这个内存,正式使用之后改为内存池分配
        friend class FunctionWrapper;

        FunctionPrivate();

        FunctionPrivate(const FunctionPrivate&);

        FunctionPrivate(FunctionPrivate&&) noexcept;

        template<typename Func>
        FunctionPrivate(Func)
        {
            using Ret = typename FunctionTraits<Func>::ReturnType;
            using ArgsTuple = typename FunctionTraits<Func>::BareTupleType;

            argsHelper = &Manager<Ret,ArgsTuple>::setArgs;
            stringArgsHelper = &Manager<Ret,ArgsTuple>::setStringArgs;
            deleteHelper = &Manager<Ret,ArgsTuple>::deleteMembers;
            resultHelper = &Manager<Ret,ArgsTuple>::result;

            //构造函数不对保存参数和结果的指针分配内存,仅仅在需要往这两个指针中写入数据时才初始化,避免对为设置参数的FunctionWrapper调用exec()引起UB
            //这两个指针要么为空表示没有保存数据,要么不为空表示已经设置好数据
            result = nullptr;
            argsTuple = nullptr;
            resultInfo = &typeid (Ret);
            argTupleInfo = &typeid(ArgsTuple);
            funcInfo = &typeid (Func);
        }

        ~FunctionPrivate();

        FunctionPrivate& operator = (const FunctionPrivate&);

        FunctionPrivate& operator = (FunctionPrivate&&) noexcept;

        bool operator == (const FunctionPrivate&);

        bool operator != (const FunctionPrivate&);

        void copyImpl(const FunctionPrivate&);

        void moveImpl(FunctionPrivate&&) noexcept;

        std::function<void(FunctionWrapper*)> functor;

        void (*argsHelper)(void*,void*&) = nullptr;
        void (*stringArgsHelper)(const QStringList&,void*&) = nullptr;
        void (*deleteHelper)(void*,void*) = nullptr;
        void (*resultHelper)(void*,void*&) = nullptr;

        void* argsTuple = nullptr;
        const std::type_info* argTupleInfo = nullptr;
        void* result = nullptr;
        const std::type_info* resultInfo = nullptr;
        const std::type_info* funcInfo = nullptr;

        QString resultString;
    }* d = nullptr;

public:
    //***可以在构造函数中加入消息id参数，打印错误信息的时候就知道是那一条消息在报错
    FunctionWrapper();

    FunctionWrapper(const FunctionWrapper& other);

    FunctionWrapper(FunctionWrapper&& other) noexcept;

    template<typename Func,typename Obj = typename FunctionTraits<Func>::Class/*,
             typename Enable = typename std::enable_if<std::is_same<Obj,typename FunctionTraits<Func>::Class>::value>::type*/>
    FunctionWrapper(Func func,Obj* obj = nullptr)
    {
        d = new FunctionPrivate(func);

        d->functor = [func,obj](FunctionWrapper* ptr)
        {
            //这里不能将this捕获到lambda中,当lambda捕获类的成员变量时,它实际上捕获的是this指针。
            //拷贝对象时,lambda中的this指针仍然指向原对象A导致新对象B执行函数时错误地访问 A 的数据。
            //例如FunctionWrapper A,然后通过拷贝构造函数创建B和C,此时B和C的functor中捕获的任然是A的this指针
            //B和C中成员变量无论如何变化,functor的执行结果都和A的执行结果一样,因为functor持有的FunctionWrapper*是A
            //所以需要将其更改成调用时传入FunctionWrapper指针
            if(ptr->d->argsTuple == nullptr)
            {
                qCritical()<<ptr->d->funcInfo->name()<<" error:no parameter has been setted,excute failed";
                return;
            }

            ptr->callHelper(func,obj);
        };
    }

    FunctionWrapper& operator = (const FunctionWrapper&) ;

    FunctionWrapper& operator = (FunctionWrapper&&) noexcept;

    ~FunctionWrapper()
    {
        delete d;
    }

    template<typename...Args>
    void operator() (Args&&...args)
    {
        exec(std::forward<Args>(args)...);
    }

    void exec()
    {
        if(d->functor.operator bool())
        {
            d->functor(this);
        }
        else
        {
            qCritical()<<d->funcInfo->name()<<" error:functor is empty,excute failed";
        }
    }

    template<typename...Args>
    void exec(Args&&...args)
    {
        setArgs(std::forward<Args>(args)...);
        this->exec();
    }

    template<typename...Args>
    typename std::enable_if<(sizeof...(Args)>0)>::type
    setArgs(Args&&...args)
    {
        using Tuple = typename std::tuple<typename std::remove_cv_t<typename std::remove_reference_t<Args>>...>;
        if(typeid (Tuple) != *d->argTupleInfo)
        {
            QString errorInfo = QString(d->funcInfo->name()) + " error:Argument type or number mismatch";
            throw std::invalid_argument(errorInfo.toStdString());
        }
        Tuple tpl = std::make_tuple(std::forward<Args>(args)...);
        this->d->argsHelper(&tpl,d->argsTuple);
    }

    //如果传入的参数列表为空就什么都不做
    template<typename...Args>
    typename std::enable_if<(sizeof...(Args)==0)>::type
    setArgs(Args&&...){}

#if CONSOLECALL
    void setStringArgs(const QStringList& args)
    {
        this->d->stringArgsHelper(args,d->argsTuple);
    }

    //这里不能使用重载来调用exec，因为exec在使用模板传参的时候可能会出现单个参数且参数类型是QStringList的时候，这种情况下会导致参数转发错误，所以需要给这两个函数改名
    void execString(const QStringList& strList)
    {
        setStringArgs(strList);
        this->exec();
    }
#endif

    template<typename RT>
    typename std::enable_if<!std::is_void<RT>::value,RT>::type getResult()
    {
        if(typeid(RT) != *d->resultInfo)
        {
            //如果类型不匹配说明代码参数错误,直接报异常
            QString errorInfo = QString(d->funcInfo->name()) + " error:return value type mismatch";
            throw std::invalid_argument(errorInfo.toStdString());
        }

        if(d->result == nullptr)
        {
            //如果是业务流程导致没有计算结果则返回一个对应的零值,同时打印错误信息
            qCritical()<<d->funcInfo->name()<<" error:incorrect return value due to nullptr result";
            return RT{};
        }
        else
            return *static_cast<RT*>(d->result);
    }

    template<std::size_t Index,typename RT = typename FunctionRT<Index>::type>
    typename std::enable_if<!std::is_void<RT>::value,RT>::type getResult()
    {
        return getResult<RT>();
    }

    template<std::size_t Index,typename RT = typename FunctionRT<Index>::type>
    typename std::enable_if<std::is_void<RT>::value>::type getResult(){}

    void getResult(void* ptr)
    {
        if(d->result == nullptr)
        {
            //如果是业务流程导致没有计算结果则返回一个对应的零值,同时打印错误信息
            qCritical()<<d->funcInfo->name()<<" error:incorrect return value due to nullptr result";
            return;
        }
        else
            d->resultHelper(d->result,ptr);
    }

#if CONSOLECALL
    QString getResultString() const noexcept
    {
        return d->resultString;
    }
#endif

private:
    ///调用成员函数
    template<typename Func,typename Obj>
    typename std::enable_if<std::is_member_function_pointer<Func>::value>::type
    callHelper(Func func,Obj* obj)
    {
        callImpl(func,obj,std::make_index_sequence<FunctionTraits<Func>::Arity>{});
    }

    ///调用非成员函数
    template<typename Func,typename Obj>
    typename std::enable_if<!std::is_member_function_pointer<Func>::value>::type
    callHelper(Func func,Obj*)
    {
        callImpl(func,std::make_index_sequence<FunctionTraits<Func>::Arity>{});
    }

    template<typename Func,std::size_t... Index,
             typename Ret = typename FunctionTraits<Func>::ReturnType,
             typename Tuple = typename FunctionTraits<Func>::BareTupleType>
    typename std::enable_if<std::is_void<Ret>::value>::type
    callImpl(Func func,std::index_sequence<Index...>)
    {
        Tuple* tpl = static_cast<Tuple*>(d->argsTuple);
        (func)(std::get<Index>(std::forward<Tuple>(*tpl))...);
    }

    template<typename Func,std::size_t... Index,
             typename Ret = typename FunctionTraits<Func>::ReturnType,
             typename Tuple = typename FunctionTraits<Func>::BareTupleType>
    typename std::enable_if<!std::is_void<Ret>::value>::type
    callImpl(Func func,std::index_sequence<Index...>)
    {
        Tuple* tpl = static_cast<Tuple*>(d->argsTuple);
        Ret value = (func)(std::get<Index>(std::forward<Tuple>(*tpl))...);
#if CONSOLECALL
        d->resultString = convertArgToString(value);
#endif
        d->resultHelper(&value,d->result);
    }

    template<typename Func,typename Obj,std::size_t... Index,
             typename Ret = typename FunctionTraits<Func>::ReturnType,
             typename Tuple = typename FunctionTraits<Func>::BareTupleType>
    typename std::enable_if<std::is_void<Ret>::value>::type
    callImpl(Func func,Obj* obj,std::index_sequence<Index...>)
    {
        Tuple* tpl = static_cast<Tuple*>(d->argsTuple);
        (obj->*func)(std::get<Index>(std::forward<Tuple>(*tpl))...);
    }

    template<typename Func,typename Obj,std::size_t... Index,
             typename Ret = typename FunctionTraits<Func>::ReturnType,
             typename Tuple = typename FunctionTraits<Func>::BareTupleType>
    typename std::enable_if<!std::is_void<Ret>::value>::type
    callImpl(Func func,Obj* obj,std::index_sequence<Index...>)
    {
        Tuple* tpl = static_cast<Tuple*>(d->argsTuple);
        Ret value = (obj->*func)(std::get<Index>(std::forward<Tuple>(*tpl))...);
#if CONSOLECALL
        d->resultString = convertArgToString(value);
#endif
        d->resultHelper(&value,d->result);
    }
};

#endif // FunctionWrapper_H

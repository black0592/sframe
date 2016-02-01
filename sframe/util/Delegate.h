
#ifndef SFRAME_DELEGATE_H
#define SFRAME_DELEGATE_H

#include <assert.h>
#include <memory.h>

namespace sframe {


// ��Tupleʵ��
template<typename... Elements>
class Tuple;

template<typename Head, typename... Tail>
class Tuple < Head, Tail... >
{
public:

	typename std::decay<Head>::type & GetHead()
	{
		return _head;
	}

	Tuple<Tail...> & GetTail()
	{
		return _tail;
	}

private:
	typename std::decay<Head>::type _head;
	Tuple<Tail...> _tail;
};

template<>
class Tuple<>
{};

// Delegate����
enum DelegateType 
{
	kDelegateType_StaticFunctionDelegate = 1,      // ��̬����ί��
	kDelegateType_MemberFunctionDelegate,          // ���Ա����ί��
};

// Delegate�ӿ�
template<typename Decoder_Type>
class IDelegate
{
public:
	virtual ~IDelegate(){}
	virtual DelegateType GetType() const = 0;
	virtual bool Call(Decoder_Type& decoder) = 0;
};

// ��̬����ί��
template<typename Decoder_Type, typename... Args_Type>
class StaticFunctionDelegate : public IDelegate<Decoder_Type>
{
public:
	typedef void(*FuncT)(Args_Type...);

public:
	StaticFunctionDelegate(FuncT func) : _func(func) {}
	~StaticFunctionDelegate() {}

	DelegateType GetType() const
	{
		return kDelegateType_StaticFunctionDelegate;
	}

	bool Call(Decoder_Type& decoder)
	{
		Tuple<Args_Type...> args;
		return this->CallTuple(decoder, args);
	}

private:
	template<typename... Head, typename... Tail>
	inline bool CallTuple(Decoder_Type& s, Tuple<Tail...>& t, Head&&... head)
	{
		return this->CallTuple(s, t.GetTail(), std::forward<Head>(head)..., t.GetHead());
	}

	template<typename... Args_T>
	inline bool CallTuple(Decoder_Type& s, Tuple<>& t, Args_T&&... args)
	{
		if (!s.Decode(args...))
		{
			assert(false);
			return false;
		}
		this->_func(std::forward<Args_T>(args)...);
		return true;
	}

private:
	FuncT _func;
};

// ��Ա����ί�нӿ�
template<typename Decoder_Type, typename Object_Type>
class IMemberFunctionDelegate : public IDelegate<Decoder_Type>
{
public:
	virtual ~IMemberFunctionDelegate() {}

	virtual bool CallWithObject(Object_Type * obj, Decoder_Type& decoder) = 0;
};

// ��Ա����ί�е�ʵ��
template<typename Decoder_Type, typename Object_Type, typename... Args_Type>
class MemeberFunctionDelegate : public IMemberFunctionDelegate<Decoder_Type, Object_Type>
{
public:
	typedef void(Object_Type::*FuncT)(Args_Type...);

public:
	MemeberFunctionDelegate(FuncT func, Object_Type * obj = nullptr) : _func(func), _obj(obj), _cur_obj(nullptr){}
	~MemeberFunctionDelegate(){}

	DelegateType GetType() const
	{
		return kDelegateType_MemberFunctionDelegate;
	}

	bool Call(Decoder_Type& decoder)
	{
		assert(_obj);
		_cur_obj = _obj;
		Tuple<Args_Type...> args;
		return this->CallTuple(decoder, args);
	}

	bool CallWithObject(Object_Type * obj, Decoder_Type& decoder)
	{
		_cur_obj = obj;
		Tuple<Args_Type...> args;
		return this->CallTuple(decoder, args);
	}

private:
	template<typename... Head, typename... Tail>
	inline bool CallTuple(Decoder_Type& s, Tuple<Tail...>& t, Head&&... head)
	{
		return this->CallTuple(s, t.GetTail(), std::forward<Head>(head)..., t.GetHead());
	}

	template<typename... Args_T>
	inline bool CallTuple(Decoder_Type& s, Tuple<>& t, Args_T&&... args)
	{
		if (!s.Decode(args...))
		{
			assert(false);
			return false;
		}
		(_cur_obj->*_func)(std::forward<Args_T>(args)...);
		return true;
	}

private:
	Object_Type * _cur_obj;
	Object_Type * const _obj;
	FuncT _func;
};


// Delegate������
template <typename Decoder_Type, int Max_Id>
class DelegateManager
{
public:
	DelegateManager()
	{
		memset(_callers, 0, sizeof(_callers));
	}

	~DelegateManager()
	{
		for (int i = 0; i < Max_Id + 1; i++)
		{
			if (_callers[i])
			{
				delete _callers[i];
			}
		}
	}

	bool Call(int id, Decoder_Type & decoder)
	{
		if (id < 0 || id > Max_Id || !_callers[id])
		{
			return false;
		}

		return _callers[id]->Call(decoder);
	}

	template<typename Object_Type>
	bool CallWithObject(int id, Object_Type * obj, Decoder_Type & decoder)
	{
		if (id < 0 || id > Max_Id || !_callers[id] || 
			_callers[id]->GetType() != kDelegateType_MemberFunctionDelegate)
		{
			return false;
		}

		IMemberFunctionDelegate<Decoder_Type, Object_Type> * member_func_caller = dynamic_cast<IMemberFunctionDelegate<Decoder_Type, Object_Type>*>(_callers[id]);
		assert(member_func_caller);
		return member_func_caller->CallWithObject(obj, decoder);
	}

	////////// ע�ắ�� ////////////

	// ע�ᾲ̬����
	template<typename... Args>
	bool Regist(int id, void(*func)(Args...))
	{
		auto caller = new StaticFunctionDelegate<Decoder_Type, Args...>(func);
		assert(caller);
		return RegistCaller(id, caller);
	}

	// ע���Ա����
	template<typename Object_Type, typename... Args>
	bool Regist(int id, void(Object_Type::*func)(Args...))
	{
		auto caller = new MemeberFunctionDelegate<Decoder_Type, Object_Type, Args...>(func);
		assert(caller);
		return RegistCaller(id, caller);
	}

	// ע���Ա����ͬʱ�󶨶���
	template<typename Object_Type, typename... Args>
	bool Regist(int id, void(Object_Type::*func)(Args...), Object_Type * obj)
	{
		auto caller = new MemeberFunctionDelegate<Decoder_Type, Object_Type, Args...>(func, obj);
		assert(caller);
		return RegistCaller(id, caller);
	}

private:
	bool RegistCaller(int id, IDelegate<Decoder_Type> * caller)
	{
		if (id < 0 || id > Max_Id || _callers[id])
		{
			delete caller;
			return false;
		}

		_callers[id] = caller;
		return true;
	}

private:
	IDelegate<Decoder_Type> * _callers[Max_Id + 1];
};

}

#endif
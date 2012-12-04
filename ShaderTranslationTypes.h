//
// Copyright 2012 Stoyan Nikolov. All rights reserved.
// Licensed under: http://www.opensource.org/licenses/BSD-2-Clause
//
#pragma once

namespace translator
{

class TlsScratchAllocator : boost::noncopyable
{
public:
	static bool Create(unsigned size)
	{
		if(sPtr.get())
		{
			return false;
		}
		sPtr.reset(new TlsScratchAllocator(size));
		return true;
	}

	static bool Destroy()
	{
	   	if(!sPtr.get())
		{
			return false;
		}
		sPtr.reset(nullptr);
		return true;
	}

	static TlsScratchAllocator* Get()
	{
		return sPtr.get();
	}

	void* Allocate(unsigned bytes)
	{
		auto ptr = m_Ptr;
		m_Ptr += bytes;
		if(m_Ptr >= m_Size)
		{
			throw std::bad_alloc();
		}
		return m_Memory.get() + ptr;
	}

	void Deallocate(void* ptr)
	{}

private:
	TlsScratchAllocator(unsigned size)
		: m_Size(size)
		, m_Ptr(0)
		, m_Memory(new char[size])
	{}

	~TlsScratchAllocator() {}

	std::unique_ptr<char[]> m_Memory;
	unsigned m_Size;
	unsigned m_Ptr;

	friend class boost::thread_specific_ptr<TlsScratchAllocator>;
	static boost::thread_specific_ptr<TlsScratchAllocator> sPtr;
};

template<typename T>
class StdAllocator {
public : 
    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

public : 
    template<typename U>
    struct rebind {
        typedef StdAllocator<U> other;
    };

public : 
    inline explicit StdAllocator(bool scratch = false)
		: m_IsScratch(scratch)
	{}
    
	inline ~StdAllocator() {}
    inline StdAllocator(const StdAllocator& rhs)
		: m_IsScratch(rhs.m_IsScratch)
	{}

    template<typename U>
    inline explicit StdAllocator(const StdAllocator<U>& rhs)
		: m_IsScratch(rhs.m_IsScratch) 
	{}

    inline pointer address(reference r) { return &r; }
    inline const_pointer address(const_reference r) { return &r; }

    inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0) 
	{ 
		if(m_IsScratch)
		{
			auto allocator = TlsScratchAllocator::Get();
			assert(allocator);
			return reinterpret_cast<pointer>(allocator->Allocate(cnt * sizeof (T)));
		}
		else
		{
			return reinterpret_cast<pointer>(malloc(cnt * sizeof (T)));
		}
    }
    inline void deallocate(pointer p, size_type) 
	{
		if(m_IsScratch)
		{
			auto allocator = TlsScratchAllocator::Get();
			assert(allocator);
			allocator->Deallocate(p);
		}
		else
		{
			::free(p);
		}
    }

    inline size_type max_size() const 
	{ 
        return std::numeric_limits<size_type>::max() / sizeof(T);
	}

    inline void construct(pointer p, const T& t) { new(p) T(t); }
    inline void destroy(pointer p) { p->~T(); }

    inline bool operator==(StdAllocator const&) { return true; }
    inline bool operator!=(StdAllocator const& a) { return !operator==(a); }

private:
	template<typename U> friend class StdAllocator;
	bool m_IsScratch;
};

typedef std::basic_string<char, std::char_traits<char>, StdAllocator<char>> String;

class ScratchString : public String
{
public:
	ScratchString()
		: String(allocator_type(true))
	{}

	ScratchString(const ScratchString& rhs)
		: String(rhs)
	{}

	ScratchString(ScratchString&& rhs)
		: String(std::move(rhs))
	{}

	explicit ScratchString(const String& rhs)
		: String(rhs.cbegin(), rhs.cend(), allocator_type(true))
	{}

	ScratchString(const char* ptr, size_type count)
		: String(ptr, count, allocator_type(true))
	{}

	ScratchString(const char* ptr)
		: String(ptr, allocator_type(true))
	{}

	template<class _Iter>
	ScratchString(_Iter first, _Iter last)
		: String(first, last, allocator_type(true))
	{}

	ScratchString& operator=(const ScratchString& rhs)
	{
		String::operator=(rhs);
		return *this;
	}

	ScratchString& operator=(ScratchString&& rhs)
	{
		String::operator=(std::forward<String>(rhs));
		return *this;
	}
};

}

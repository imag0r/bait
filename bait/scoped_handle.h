#pragma once

template<HANDLE _InvalidHandleValue>
class scoped_handle_impl
{
public:
    scoped_handle_impl()
        : handle_(_InvalidHandleValue)
    {
    }

    scoped_handle_impl(HANDLE handle)
        : handle_(handle)
    {
    }

	scoped_handle_impl(const scoped_handle_impl&) = delete;

    scoped_handle_impl& operator=(HANDLE handle)
    {
        close();
        handle_ = handle;
        return *this;
    }

	scoped_handle_impl& operator=(const scoped_handle_impl&) = delete;

    ~scoped_handle_impl()
    {
        close();
    }

    inline bool valid() const throw()
    {
        return (_InvalidHandleValue != handle_);
    }

    inline operator const HANDLE() const throw()
    {
        return handle_;
    }

    inline void close() throw()
    {
        if (valid())
        {
            ::CloseHandle(handle_);
            handle_ = _InvalidHandleValue;
        }
    }

private:
    HANDLE handle_;
};

typedef scoped_handle_impl<(HANDLE)(LONG_PTR)(NULL)> scoped_handle;
typedef scoped_handle_impl<INVALID_HANDLE_VALUE> scoped_file_handle;

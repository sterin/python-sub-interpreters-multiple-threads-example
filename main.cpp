// Author: Baruch Sterin <baruchs@gmail.com>

#include <Python.h>

#include <string>
#include <vector>
#include <thread>

// initialize and clean up python
struct initialize
{
    initialize()
    {
        Py_InitializeEx(1);
        PyEval_InitThreads(); // not needed as of Python 3.7, deprecated as of 3.9
    }

    ~initialize()
    {
        Py_Finalize();
    }
};

// allow other threads to run
class enable_threads_scope
{
public:
    enable_threads_scope()
    {
        _state = PyEval_SaveThread();
    }

    ~enable_threads_scope()
    {
        PyEval_RestoreThread(_state);
    }

private:

    PyThreadState* _state;
};

// restore the thread state when the object goes out of scope
class restore_tstate_scope
{
public:

    restore_tstate_scope()
    {
        _ts = PyThreadState_Get();
    }

    ~restore_tstate_scope()
    {
        PyThreadState_Swap(_ts);
    }

private:

    PyThreadState* _ts;
};

// swap the current thread state with ts, restore when the object goes out of scope
class swap_tstate_scope
{
public:

    swap_tstate_scope(PyThreadState* ts)
    {
        _ts = PyThreadState_Swap(ts);
    }

    ~swap_tstate_scope()
    {
        PyThreadState_Swap(_ts);
    }

private:

    PyThreadState* _ts;
};

// create new thread state for interpreter interp, make it current, and clean up on destruction
class thread_state
{
public:

    thread_state(PyInterpreterState* interp)
    {
        _ts = PyThreadState_New(interp);
        PyEval_RestoreThread(_ts);
    }

    ~thread_state()
    {
        PyThreadState_Clear(_ts);
        PyThreadState_DeleteCurrent();
    }

    operator PyThreadState*()
    {
        return _ts;
    }

    static PyThreadState* current()
    {
        return PyThreadState_Get();
    }

private:

    PyThreadState* _ts;
};

// represent a sub interpreter
class sub_interpreter
{
public:

    // perform the necessary setup and cleanup for a new thread running using a specific interpreter
    struct thread_scope
    {
        thread_state _state;
        swap_tstate_scope _swap{ _state };

        thread_scope(PyInterpreterState* interp) :
            _state(interp)
        {
        }
    };

    sub_interpreter()
    {
        restore_tstate_scope restore;

        _ts = Py_NewInterpreter();
    }

    ~sub_interpreter()
    {
        if( _ts )
        {
            swap_tstate_scope sts(_ts);

            Py_EndInterpreter(_ts);
        }
    }

    PyInterpreterState* interp()
    {
        return _ts->interp;
    }

    static PyInterpreterState* current()
    {
        return thread_state::current()->interp;
    }

private:

    PyThreadState* _ts;
};

// runs in a new thread
void f(PyInterpreterState* interp, const char* tname)
{
    std::string code = R"PY(

from __future__ import print_function
import sys

print("TNAME: sys.xxx={}".format(getattr(sys, 'xxx', 'attribute not set')))

    )PY";

    code.replace(code.find("TNAME"), 5, tname);

    sub_interpreter::thread_scope scope(interp);
    PyRun_SimpleString(code.c_str());
}

int main()
{
    initialize init;

    sub_interpreter s1;
    sub_interpreter s2;

    PyRun_SimpleString(R"PY(

# set sys.xxx, it will only be reflected in t4, which runs in the context of the main interpreter

from __future__ import print_function
import sys

sys.xxx = ['abc']
print('main: setting sys.xxx={}'.format(sys.xxx))

    )PY");

    std::thread t1{f, s1.interp(), "t1(s1)"};
    std::thread t2{f, s2.interp(), "t2(s2)"};
    std::thread t3{f, s1.interp(), "t3(s1)"};
    std::thread t4{f, sub_interpreter::current(), "t4(main)"};

    enable_threads_scope t;

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    return 0;
}
